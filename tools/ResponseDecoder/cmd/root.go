package cmd

import (
	"fmt"
	"os"
	"path/filepath"

	"github.com/spensercai/nfc_apdu_runner/tools/ResponseDecoder/pkg/decoder"
	"github.com/spensercai/nfc_apdu_runner/tools/ResponseDecoder/pkg/flipper"
	"github.com/spf13/cobra"
)

var (
	filePath     string
	useDevice    bool
	useSerial    bool
	serialPort   string
	formatPath   string
	formatDir    string
	responseData string
)

// rootCmd represents the base command when called without any subcommands
var rootCmd = &cobra.Command{
	Use:   "response_decoder",
	Short: "APDU Response Decoder for Flipper Zero",
	Long: `APDU Response Decoder is a tool for parsing and displaying .apdures files
from Flipper Zero NFC APDU Runner application.

You can load .apdures files either from a local file or directly from your Flipper Zero device
using either SD card mounting or serial communication.`,
	RunE: func(cmd *cobra.Command, args []string) error {
		// 获取响应数据
		var err error
		if useDevice {
			// 从设备获取响应数据
			if useSerial {
				// 使用串口通信
				responseData, err = flipper.GetResponseFromDeviceSerial(serialPort)
			} else {
				// 使用SD卡挂载
				responseData, err = flipper.GetResponseFromDevice()
			}
			if err != nil {
				return fmt.Errorf("failed to get response from device: %w", err)
			}
		} else if filePath != "" {
			// 从文件获取响应数据
			data, err := os.ReadFile(filePath)
			if err != nil {
				return fmt.Errorf("failed to read file: %w", err)
			}
			responseData = string(data)
		} else {
			return fmt.Errorf("either --file or --device flag must be specified")
		}

		// 解析响应数据
		response, err := decoder.ParseResponseData(responseData)
		if err != nil {
			return fmt.Errorf("failed to parse response data: %w", err)
		}

		// 如果没有指定格式文件，列出可用的格式文件
		if formatPath == "" {
			// 获取格式目录下的所有.apdufmt文件
			formats, err := decoder.ListFormatFiles(formatDir)
			if err != nil {
				return fmt.Errorf("failed to list format files: %w", err)
			}

			// 让用户选择格式文件
			selectedFormat, err := decoder.SelectFormat(formats)
			if err != nil {
				return fmt.Errorf("failed to select format: %w", err)
			}

			formatPath = filepath.Join(formatDir, selectedFormat)
		}

		// 读取格式文件
		formatData, err := os.ReadFile(formatPath)
		if err != nil {
			return fmt.Errorf("failed to read format file: %w", err)
		}

		// 解码并显示结果
		err = decoder.DecodeAndDisplay(response, string(formatData))
		if err != nil {
			return fmt.Errorf("failed to decode and display: %w", err)
		}

		return nil
	},
}

// Execute adds all child commands to the root command and sets flags appropriately.
// This is called by main.main(). It only needs to happen once to the rootCmd.
func Execute() error {
	return rootCmd.Execute()
}

func init() {
	// 获取当前可执行文件所在目录
	execPath, err := os.Executable()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error getting executable path: %s\n", err)
		os.Exit(1)
	}
	execDir := filepath.Dir(execPath)

	// 设置默认格式目录
	formatDir = filepath.Join(execDir, "format")

	// 如果格式目录不存在，尝试使用相对路径
	if _, err := os.Stat(formatDir); os.IsNotExist(err) {
		formatDir = "format"
	}

	// 添加命令行标志
	rootCmd.Flags().StringVarP(&filePath, "file", "f", "", "Path to .apdures file")
	rootCmd.Flags().BoolVarP(&useDevice, "device", "d", false, "Connect to Flipper Zero device")
	rootCmd.Flags().BoolVarP(&useSerial, "serial", "s", false, "Use serial communication with Flipper Zero (requires -d)")
	rootCmd.Flags().StringVarP(&serialPort, "port", "p", "", "Specify serial port for Flipper Zero (optional)")
	rootCmd.Flags().StringVar(&formatPath, "decode-format", "", "Path to format template file (.apdufmt)")
	rootCmd.Flags().StringVar(&formatDir, "format-dir", formatDir, "Directory containing format template files")

	// 禁用completion命令
	rootCmd.CompletionOptions.DisableDefaultCmd = true
}
