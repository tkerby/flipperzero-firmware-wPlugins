package flipper

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"go.bug.st/serial"
	"go.bug.st/serial/enumerator"
)

const (
	// Flipper Zero的串口参数
	baudRate     = 115200
	dataBits     = 8
	stopBits     = serial.OneStopBit
	parity       = serial.NoParity
	readTimeout  = 5 * time.Second
	writeTimeout = 5 * time.Second

	// Flipper Zero CLI命令
	cmdPrompt       = ">: "
	cmdListFiles    = "storage list %s"
	cmdReadFile     = "storage read %s"
	cmdExit         = "exit"
	apduResponseDir = "/ext/apps_data/nfc_apdu_runner"

	// Flipper Zero USB VID/PID
	flipperVID = "0483"
	flipperPID = "5740"

	// 默认挂载路径（macOS）
	defaultMountPath = "/Volumes/Flipper SD"
)

// GetResponseFromDevice 从Flipper Zero设备获取APDU响应数据
// 通过SD卡挂载方式
func GetResponseFromDevice() (string, error) {
	// 获取Flipper Zero挂载路径
	mountPath, err := getFlipperMountPath()
	if err != nil {
		return "", fmt.Errorf("failed to get Flipper Zero mount path: %w", err)
	}

	// 列出响应文件
	files, err := listResponseFilesSD(mountPath)
	if err != nil {
		return "", fmt.Errorf("failed to list response files: %w", err)
	}

	if len(files) == 0 {
		return "", errors.New("no response files found on Flipper Zero")
	}

	// 让用户选择文件
	selectedFile, err := selectFile(files)
	if err != nil {
		return "", fmt.Errorf("failed to select file: %w", err)
	}

	// 读取文件内容
	content, err := readFileFromDeviceSD(mountPath, selectedFile)
	if err != nil {
		return "", fmt.Errorf("failed to read file from device: %w", err)
	}

	return content, nil
}

// GetResponseFromDeviceSerial 从Flipper Zero设备获取APDU响应数据
// 通过串口通信方式
func GetResponseFromDeviceSerial(portName string) (string, error) {
	// 连接到Flipper Zero
	port, err := connectToFlipper(portName)
	if err != nil {
		return "", fmt.Errorf("failed to connect to Flipper Zero: %w", err)
	}
	defer port.Close()

	// 列出响应文件
	files, err := listResponseFilesSerial(port)
	if err != nil {
		return "", fmt.Errorf("failed to list response files: %w", err)
	}

	if len(files) == 0 {
		return "", errors.New("no response files found on Flipper Zero")
	}

	// 让用户选择文件
	selectedFile, err := selectFile(files)
	if err != nil {
		return "", fmt.Errorf("failed to select file: %w", err)
	}

	// 读取文件内容
	content, err := readFileFromDeviceSerial(port, selectedFile)
	if err != nil {
		return "", fmt.Errorf("failed to read file from device: %w", err)
	}

	return content, nil
}

// connectToFlipper 连接到Flipper Zero设备
func connectToFlipper(portName string) (serial.Port, error) {
	// 如果没有指定端口，自动检测Flipper Zero端口
	if portName == "" {
		var err error
		portName, err = findFlipperPort()
		if err != nil {
			return nil, err
		}
	}

	// 配置串口参数
	mode := &serial.Mode{
		BaudRate: baudRate,
		DataBits: dataBits,
		StopBits: stopBits,
		Parity:   parity,
	}

	// 打开串口
	port, err := serial.Open(portName, mode)
	if err != nil {
		return nil, fmt.Errorf("failed to open serial port %s: %w", portName, err)
	}

	// 设置超时
	if err := port.SetReadTimeout(readTimeout); err != nil {
		port.Close()
		return nil, fmt.Errorf("failed to set read timeout: %w", err)
	}

	fmt.Printf("Connected to Flipper Zero on %s\n", portName)
	return port, nil
}

// findFlipperPort 查找Flipper Zero的串口
func findFlipperPort() (string, error) {
	// 获取所有串口
	ports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		return "", fmt.Errorf("failed to get serial ports: %w", err)
	}

	// 查找Flipper Zero的串口
	for _, port := range ports {
		// 根据VID和PID识别Flipper Zero
		if (port.IsUSB && port.VID == "0483" && port.PID == "5740") || // Flipper Zero的VID和PID
			strings.Contains(strings.ToLower(port.Product), "flipper") ||
			strings.Contains(strings.ToLower(port.Name), "flipper") {
			return port.Name, nil
		}
	}

	// 如果没有找到，尝试根据操作系统的常见命名规则查找
	var potentialPorts []string
	switch runtime.GOOS {
	case "windows":
		// Windows上通常是COMx
		for _, port := range ports {
			if strings.HasPrefix(port.Name, "COM") {
				potentialPorts = append(potentialPorts, port.Name)
			}
		}
	case "darwin":
		// macOS上通常是/dev/tty.usbmodem*
		for _, port := range ports {
			if strings.Contains(port.Name, "usbmodem") {
				potentialPorts = append(potentialPorts, port.Name)
			}
		}
	case "linux":
		// Linux上通常是/dev/ttyACM*
		for _, port := range ports {
			if strings.Contains(port.Name, "ttyACM") {
				potentialPorts = append(potentialPorts, port.Name)
			}
		}
	}

	// 如果找到了潜在的端口，使用第一个
	if len(potentialPorts) > 0 {
		return potentialPorts[0], nil
	}

	return "", errors.New("Flipper Zero device not found")
}

// flushPort 清空串口缓冲区
func flushPort(port serial.Port) error {
	buffer := make([]byte, 1024)
	for {
		// 设置较短的超时，以便快速返回
		port.SetReadTimeout(100 * time.Millisecond)
		n, err := port.Read(buffer)
		if err != nil || n == 0 {
			break
		}
	}
	// 恢复正常超时
	return port.SetReadTimeout(readTimeout)
}

// sendCommand 发送命令到Flipper Zero
func sendCommand(port serial.Port, command string) error {
	// 添加回车符
	command = command + "\r"
	_, err := port.Write([]byte(command))
	return err
}

// waitForPrompt 等待CLI提示符
func waitForPrompt(port serial.Port) (string, error) {
	var buffer bytes.Buffer
	readBuffer := make([]byte, 1)
	startTime := time.Now()

	for {
		// 检查是否超时
		if time.Since(startTime) > readTimeout {
			return buffer.String(), errors.New("timeout waiting for prompt")
		}

		// 读取一个字节
		n, err := port.Read(readBuffer)
		if err != nil {
			return buffer.String(), err
		}

		if n > 0 {
			buffer.WriteByte(readBuffer[0])

			// 检查是否收到提示符
			if buffer.Len() >= len(cmdPrompt) &&
				bytes.HasSuffix(buffer.Bytes(), []byte(cmdPrompt)) {
				return buffer.String(), nil
			}
		}
	}
}

// readResponse 读取响应直到提示符出现
func readResponse(port serial.Port) (string, error) {
	return waitForPrompt(port)
}

// listResponseFiles 列出设备上的.apdures文件
func listResponseFiles(port serial.Port) ([]string, error) {
	// 发送列出文件的命令
	err := sendCommand(port, fmt.Sprintf(cmdListFiles, apduResponseDir))
	if err != nil {
		return nil, fmt.Errorf("failed to send list command: %w", err)
	}

	// 读取响应
	response, err := readResponse(port)
	if err != nil {
		return nil, fmt.Errorf("failed to read response: %w", err)
	}

	// 解析响应，提取文件列表
	var files []string
	scanner := bufio.NewScanner(strings.NewReader(response))
	for scanner.Scan() {
		line := scanner.Text()
		// 跳过命令行和提示符
		if strings.Contains(line, cmdListFiles) || strings.Contains(line, cmdPrompt) {
			continue
		}
		// 检查是否是.apdures文件
		if strings.HasSuffix(line, ".apdures") {
			// 提取文件名
			parts := strings.Fields(line)
			if len(parts) > 0 {
				fileName := parts[len(parts)-1]
				files = append(files, filepath.Join(apduResponseDir, fileName))
			}
		}
	}

	return files, nil
}

// selectFile 让用户选择文件
func selectFile(files []string) (string, error) {
	fmt.Println("Available .apdures files:")
	for i, file := range files {
		// 显示文件名而不是完整路径
		fmt.Printf("%d. %s\n", i+1, filepath.Base(file))
	}

	fmt.Print("Select a file (1-", len(files), "): ")
	var choice int
	_, err := fmt.Scanf("%d", &choice)
	if err != nil {
		return "", fmt.Errorf("error reading user input: %w", err)
	}

	if choice < 1 || choice > len(files) {
		return "", fmt.Errorf("invalid choice: %d", choice)
	}

	return files[choice-1], nil
}

// readFileFromDevice 从设备读取文件内容
func readFileFromDevice(port serial.Port, filePath string) (string, error) {
	// 发送读取文件的命令
	err := sendCommand(port, fmt.Sprintf(cmdReadFile, filePath))
	if err != nil {
		return "", fmt.Errorf("failed to send read command: %w", err)
	}

	// 读取响应
	response, err := readResponse(port)
	if err != nil {
		return "", fmt.Errorf("failed to read response: %w", err)
	}

	// 处理响应，提取文件内容
	var content strings.Builder
	scanner := bufio.NewScanner(strings.NewReader(response))
	var inContent bool
	for scanner.Scan() {
		line := scanner.Text()
		// 跳过命令行
		if strings.Contains(line, cmdReadFile) {
			inContent = true
			continue
		}
		// 结束于提示符
		if strings.Contains(line, cmdPrompt) {
			break
		}
		// 收集内容
		if inContent {
			content.WriteString(line)
			content.WriteString("\n")
		}
	}

	return content.String(), nil
}

// CloseConnection 关闭与Flipper Zero的连接
func CloseConnection(port serial.Port) error {
	if port == nil {
		return nil
	}

	// 发送退出命令
	err := sendCommand(port, cmdExit)
	if err != nil {
		return fmt.Errorf("failed to send exit command: %w", err)
	}

	// 关闭端口
	return port.Close()
}

// listResponseFilesSerial 通过串口列出响应文件
func listResponseFilesSerial(port serial.Port) ([]string, error) {
	// 发送命令列出文件
	cmd := fmt.Sprintf("storage list %s\r\n", apduResponseDir)
	if _, err := port.Write([]byte(cmd)); err != nil {
		return nil, fmt.Errorf("failed to send command: %w", err)
	}

	// 读取响应
	response := make([]byte, 4096)
	n, err := port.Read(response)
	if err != nil {
		return nil, fmt.Errorf("failed to read response: %w", err)
	}

	// 解析响应，提取文件名
	responseStr := string(response[:n])
	var files []string
	scanner := bufio.NewScanner(strings.NewReader(responseStr))
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasSuffix(line, ".apdures") {
			// 提取文件名
			fileName := filepath.Base(line)
			files = append(files, fileName)
		}
	}

	return files, nil
}

// readFileFromDeviceSerial 通过串口从设备读取文件内容
func readFileFromDeviceSerial(port serial.Port, fileName string) (string, error) {
	// 发送命令读取文件
	filePath := filepath.Join(apduResponseDir, fileName)
	cmd := fmt.Sprintf("storage read %s\r\n", filePath)
	if _, err := port.Write([]byte(cmd)); err != nil {
		return "", fmt.Errorf("failed to send command: %w", err)
	}

	// 读取响应
	var buffer bytes.Buffer
	readBuf := make([]byte, 1024)

	// 设置超时
	deadline := time.Now().Add(readTimeout)

	for time.Now().Before(deadline) {
		n, err := port.Read(readBuf)
		if err != nil {
			if err == io.EOF {
				break
			}
			return "", fmt.Errorf("failed to read response: %w", err)
		}

		if n == 0 {
			// 没有更多数据
			break
		}

		buffer.Write(readBuf[:n])

		// 检查是否读取完成
		if bytes.Contains(readBuf[:n], []byte("storage read done")) {
			break
		}
	}

	// 提取文件内容
	content := buffer.String()
	// 移除命令回显和结束标记
	content = strings.Replace(content, cmd, "", 1)
	content = strings.Replace(content, "storage read done", "", 1)

	return strings.TrimSpace(content), nil
}

// getFlipperMountPath 获取Flipper Zero挂载路径
func getFlipperMountPath() (string, error) {
	switch runtime.GOOS {
	case "darwin":
		return getMacOSMountPath()
	case "windows":
		return getWindowsMountPath()
	case "linux":
		return getLinuxMountPath()
	default:
		return "", fmt.Errorf("unsupported operating system: %s", runtime.GOOS)
	}
}

// getMacOSMountPath 获取macOS下的挂载路径
func getMacOSMountPath() (string, error) {
	// 检查默认路径是否存在
	if _, err := os.Stat(defaultMountPath); err == nil {
		return defaultMountPath, nil
	}

	// 使用diskutil命令查找挂载的Flipper SD卡
	cmd := exec.Command("diskutil", "list", "-plist")
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("failed to execute diskutil command: %w", err)
	}

	// 解析输出，查找Flipper SD卡
	// 这里简化处理，实际应该解析plist格式
	if strings.Contains(string(output), "Flipper SD") {
		// 查找挂载点
		cmd = exec.Command("mount")
		output, err = cmd.Output()
		if err != nil {
			return "", fmt.Errorf("failed to execute mount command: %w", err)
		}

		// 查找包含"Flipper SD"的行
		scanner := bufio.NewScanner(strings.NewReader(string(output)))
		for scanner.Scan() {
			line := scanner.Text()
			if strings.Contains(line, "Flipper SD") {
				// 提取挂载点
				parts := strings.Split(line, " on ")
				if len(parts) > 1 {
					mountPoint := strings.Split(parts[1], " (")[0]
					return mountPoint, nil
				}
			}
		}
	}

	return "", errors.New("Flipper SD card not found. Please connect your Flipper Zero and ensure the SD card is mounted")
}

// getWindowsMountPath 获取Windows下的挂载路径
func getWindowsMountPath() (string, error) {
	// 使用wmic命令查找挂载的Flipper SD卡
	cmd := exec.Command("wmic", "logicaldisk", "where", "VolumeName='Flipper SD'", "get", "DeviceID")
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("failed to execute wmic command: %w", err)
	}

	// 解析输出，查找Flipper SD卡
	scanner := bufio.NewScanner(strings.NewReader(string(output)))
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line != "" && line != "DeviceID" {
			return line, nil
		}
	}

	return "", errors.New("Flipper SD card not found. Please connect your Flipper Zero and ensure the SD card is mounted")
}

// getLinuxMountPath 获取Linux下的挂载路径
func getLinuxMountPath() (string, error) {
	// 使用lsblk命令查找挂载的Flipper SD卡
	cmd := exec.Command("lsblk", "-o", "NAME,LABEL,MOUNTPOINT", "-J")
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("failed to execute lsblk command: %w", err)
	}

	// 解析输出，查找Flipper SD卡
	// 这里简化处理，实际应该解析JSON格式
	if strings.Contains(string(output), "Flipper SD") {
		// 查找挂载点
		cmd = exec.Command("mount")
		output, err = cmd.Output()
		if err != nil {
			return "", fmt.Errorf("failed to execute mount command: %w", err)
		}

		// 查找包含"Flipper SD"的行
		scanner := bufio.NewScanner(strings.NewReader(string(output)))
		for scanner.Scan() {
			line := scanner.Text()
			if strings.Contains(line, "Flipper SD") {
				// 提取挂载点
				parts := strings.Split(line, " on ")
				if len(parts) > 1 {
					mountPoint := strings.Split(parts[1], " type ")[0]
					return mountPoint, nil
				}
			}
		}
	}

	return "", errors.New("Flipper SD card not found. Please connect your Flipper Zero and ensure the SD card is mounted")
}

// listResponseFilesSD 列出SD卡上的.apdures文件
func listResponseFilesSD(mountPath string) ([]string, error) {
	// 构建APDU响应文件目录路径
	dirPath := filepath.Join(mountPath, apduResponseDir)

	// 检查目录是否存在
	if _, err := os.Stat(dirPath); os.IsNotExist(err) {
		return nil, fmt.Errorf("APDU response directory not found: %s", dirPath)
	}

	// 读取目录内容
	entries, err := os.ReadDir(dirPath)
	if err != nil {
		return nil, fmt.Errorf("failed to read directory: %w", err)
	}

	// 过滤.apdures文件
	var files []string
	for _, entry := range entries {
		if !entry.IsDir() && strings.HasSuffix(entry.Name(), ".apdures") {
			files = append(files, filepath.Join(apduResponseDir, entry.Name()))
		}
	}

	return files, nil
}

// readFileFromDeviceSD 从SD卡读取文件内容
func readFileFromDeviceSD(mountPath, filePath string) (string, error) {
	// 构建文件完整路径
	fullPath := filepath.Join(mountPath, filePath)

	// 检查文件是否存在
	if _, err := os.Stat(fullPath); os.IsNotExist(err) {
		return "", fmt.Errorf("file not found: %s", fullPath)
	}

	// 读取文件内容
	data, err := os.ReadFile(fullPath)
	if err != nil {
		return "", fmt.Errorf("failed to read file: %w", err)
	}

	return string(data), nil
}
