package nard

import (
	"io/ioutil"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/go-openapi/runtime/middleware"
	"github.com/go-openapi/strfmt"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/flipper"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	nardOps "github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations/nard"
)

// 获取Flipper设备处理器
func NewGetFlipperDevicesHandler() nardOps.GetFlipperDevicesHandler {
	return &getFlipperDevicesHandler{}
}

type getFlipperDevicesHandler struct{}

func (h *getFlipperDevicesHandler) Handle(params nardOps.GetFlipperDevicesParams) middleware.Responder {
	var devices []*models.FlipperDevice

	// 尝试获取SD卡挂载设备
	mountPath, err := getFlipperMountPath()
	if err == nil {
		devices = append(devices, &models.FlipperDevice{
			ID:         "flipper_sd",
			Path:       mountPath,
			SerialPort: "",
			IsSerial:   false,
		})
	}

	// 尝试获取串口设备
	serialPorts, err := flipper.GetFlipperSerialPorts()
	if err == nil {
		for _, port := range serialPorts {
			devices = append(devices, &models.FlipperDevice{
				ID:         "flipper_serial_" + port,
				Path:       "",
				SerialPort: port,
				IsSerial:   true,
			})
		}
	}

	if len(devices) == 0 {
		return nardOps.NewGetFlipperDevicesOK().WithPayload(business.ErrorResponse(404, "未找到Flipper Zero设备"))
	}

	return nardOps.NewGetFlipperDevicesOK().WithPayload(business.SuccessResponse(devices))
}

// 获取Flipper文件处理器
func NewGetFlipperFilesHandler() nardOps.GetFlipperFilesHandler {
	return &getFlipperFilesHandler{}
}

type getFlipperFilesHandler struct{}

func (h *getFlipperFilesHandler) Handle(params nardOps.GetFlipperFilesParams) middleware.Responder {
	// 获取设备路径
	devicePath := ""
	if params.DevicePath != nil {
		devicePath = *params.DevicePath
	}

	// 获取串口
	serialPort := ""
	if params.SerialPort != nil {
		serialPort = *params.SerialPort
	}

	// 是否使用串口
	useSerial := false
	if params.UseSerial != nil {
		useSerial = *params.UseSerial
	}

	if useSerial {
		// 使用串口获取文件列表
		// 即使serialPort为空，ConnectToFlipper也会自动匹配设备
		port, err := flipper.ConnectToFlipper(serialPort)
		if err != nil {
			return nardOps.NewGetFlipperFilesOK().WithPayload(business.ErrorResponse(500, "无法连接到Flipper Zero: "+err.Error()))
		}
		defer port.Close()

		files, err := flipper.ListResponseFilesSerial(port)
		if err != nil {
			return nardOps.NewGetFlipperFilesOK().WithPayload(business.ErrorResponse(500, "无法获取文件列表: "+err.Error()))
		}

		// 构建结果
		var result []*models.ApduResFile
		for _, file := range files {
			fileName := filepath.Base(file)
			result = append(result, &models.ApduResFile{
				ID:      fileName,
				Name:    fileName,
				Path:    file,
				ModTime: strfmt.DateTime{}, // 串口模式下无法获取修改时间
				Size:    0,                 // 串口模式下无法获取文件大小
			})
		}

		return nardOps.NewGetFlipperFilesOK().WithPayload(business.SuccessResponse(result))
	} else if devicePath != "" {
		// 使用设备路径获取文件列表
		apduResPath := filepath.Join(devicePath, "apps_data/nfc_apdu_runner")
		files, err := ioutil.ReadDir(apduResPath)
		if err != nil {
			return nardOps.NewGetFlipperFilesOK().WithPayload(business.ErrorResponse(404, "无法读取设备文件: "+err.Error()))
		}

		// 构建结果
		var result []*models.ApduResFile
		for _, file := range files {
			if file.IsDir() {
				continue
			}

			if filepath.Ext(file.Name()) != ".apdures" {
				continue
			}

			result = append(result, &models.ApduResFile{
				ID:      file.Name(),
				Name:    file.Name(),
				Path:    filepath.Join(devicePath, "apps_data/nfc_apdu_runner", file.Name()),
				ModTime: strfmt.DateTime(file.ModTime()),
				Size:    file.Size(),
			})
		}

		return nardOps.NewGetFlipperFilesOK().WithPayload(business.SuccessResponse(result))
	} else {
		return nardOps.NewGetFlipperFilesOK().WithPayload(business.ErrorResponse(400, "未指定设备路径或串口"))
	}
}

// 获取Flipper文件内容处理器
func NewGetFlipperFileContentHandler() nardOps.GetFlipperFileContentHandler {
	return &getFlipperFileContentHandler{}
}

type getFlipperFileContentHandler struct{}

func (h *getFlipperFileContentHandler) Handle(params nardOps.GetFlipperFileContentParams) middleware.Responder {
	// 获取文件ID
	fileID := params.FileID

	// 获取设备路径
	devicePath := ""
	if params.DevicePath != nil {
		devicePath = *params.DevicePath
	}

	// 获取串口
	serialPort := ""
	if params.SerialPort != nil {
		serialPort = *params.SerialPort
	}

	// 是否使用串口
	useSerial := false
	if params.UseSerial != nil {
		useSerial = *params.UseSerial
	}

	if useSerial {
		// 使用串口获取文件内容
		// 即使serialPort为空，ConnectToFlipper也会自动匹配设备
		port, err := flipper.ConnectToFlipper(serialPort)
		if err != nil {
			return nardOps.NewGetFlipperFileContentOK().WithPayload(business.ErrorResponse(500, "无法连接到Flipper Zero: "+err.Error()))
		}
		defer port.Close()

		content, err := flipper.ReadFileFromDeviceSerial(port, fileID)
		if err != nil {
			return nardOps.NewGetFlipperFileContentOK().WithPayload(business.ErrorResponse(500, "无法读取文件内容: "+err.Error()))
		}

		// 构建结果
		result := &models.ApduResContent{
			ID:      fileID,
			Name:    fileID,
			Content: content,
		}

		return nardOps.NewGetFlipperFileContentOK().WithPayload(business.SuccessResponse(result))
	} else if devicePath != "" {
		// 使用设备路径获取文件内容
		filePath := filepath.Join(devicePath, "apps_data/nfc_apdu_runner", fileID)
		data, err := ioutil.ReadFile(filePath)
		if err != nil {
			return nardOps.NewGetFlipperFileContentOK().WithPayload(business.ErrorResponse(404, "无法读取文件: "+err.Error()))
		}

		// 构建结果
		result := &models.ApduResContent{
			ID:      fileID,
			Name:    fileID,
			Content: string(data),
		}

		return nardOps.NewGetFlipperFileContentOK().WithPayload(business.SuccessResponse(result))
	} else {
		return nardOps.NewGetFlipperFileContentOK().WithPayload(business.ErrorResponse(400, "未指定设备路径或串口"))
	}
}

// 获取Flipper Zero挂载路径
func getFlipperMountPath() (string, error) {
	// 根据操作系统获取挂载路径
	switch runtime.GOOS {
	case "darwin":
		return getMacOSMountPath()
	case "windows":
		return getWindowsMountPath()
	case "linux":
		return getLinuxMountPath()
	default:
		return "", os.ErrNotExist
	}
}

// 获取macOS上的Flipper Zero挂载路径
func getMacOSMountPath() (string, error) {
	// 默认挂载路径
	defaultPath := "/Volumes/Flipper SD"
	if _, err := os.Stat(defaultPath); err == nil {
		return defaultPath, nil
	}

	// 查找其他可能的挂载路径
	volumes, err := ioutil.ReadDir("/Volumes")
	if err != nil {
		return "", err
	}

	for _, volume := range volumes {
		if strings.Contains(strings.ToLower(volume.Name()), "flipper") {
			return filepath.Join("/Volumes", volume.Name()), nil
		}
	}

	return "", os.ErrNotExist
}

// 获取Windows上的Flipper Zero挂载路径
func getWindowsMountPath() (string, error) {
	// 在Windows上，需要检查所有可用的驱动器
	for _, drive := range "DEFGHIJKLMNOPQRSTUVWXYZ" {
		path := string(drive) + ":\\"
		if _, err := os.Stat(path); err == nil {
			// 检查是否是Flipper Zero
			if isFlipperDrive(path) {
				return path, nil
			}
		}
	}

	return "", os.ErrNotExist
}

// 获取Linux上的Flipper Zero挂载路径
func getLinuxMountPath() (string, error) {
	// 常见的挂载点
	mountPoints := []string{
		"/media",
		"/mnt",
		"/run/media",
	}

	for _, mountPoint := range mountPoints {
		if _, err := os.Stat(mountPoint); err != nil {
			continue
		}

		users, err := ioutil.ReadDir(mountPoint)
		if err != nil {
			continue
		}

		for _, user := range users {
			userPath := filepath.Join(mountPoint, user.Name())
			if !user.IsDir() {
				continue
			}

			devices, err := ioutil.ReadDir(userPath)
			if err != nil {
				continue
			}

			for _, device := range devices {
				if !device.IsDir() {
					continue
				}

				if strings.Contains(strings.ToLower(device.Name()), "flipper") {
					return filepath.Join(userPath, device.Name()), nil
				}
			}
		}
	}

	return "", os.ErrNotExist
}

// 检查是否是Flipper Zero驱动器
func isFlipperDrive(path string) bool {
	// 检查是否存在Flipper Zero特有的目录结构
	appsDir := filepath.Join(path, "apps")
	if _, err := os.Stat(appsDir); err == nil {
		return true
	}

	// 检查卷标
	volumeName := getVolumeLabel(path)
	return strings.Contains(strings.ToLower(volumeName), "flipper")
}

// 获取卷标（简化实现）
func getVolumeLabel(path string) string {
	// 这里简化实现，实际应该使用系统API获取卷标
	return filepath.Base(path)
}
