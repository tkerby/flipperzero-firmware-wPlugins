package nard

import (
	"io/ioutil"
	"path/filepath"

	"github.com/go-openapi/runtime/middleware"
	"github.com/go-openapi/strfmt"
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
	// 获取可用的Flipper设备
	// 这里简化实现，实际应该调用flipper包中的函数获取设备列表
	devices := []*models.FlipperDevice{
		{
			ID:         "flipper_sd",
			Path:       "/Volumes/Flipper SD",
			SerialPort: "",
			IsSerial:   false,
		},
	}

	// 注意：实际实现中应该调用flipper包中的函数获取串口设备列表
	// 这里简化实现，不再尝试获取串口设备

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

	if useSerial && serialPort != "" {
		// 使用串口获取文件列表
		// 这里简化实现，实际应该调用flipper包中的函数
		return nardOps.NewGetFlipperFilesOK().WithPayload(business.ErrorResponse(500, "串口获取文件列表功能尚未实现"))
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

	if useSerial && serialPort != "" {
		// 使用串口获取文件内容
		// 这里简化实现，实际应该调用flipper包中的函数
		return nardOps.NewGetFlipperFileContentOK().WithPayload(business.ErrorResponse(500, "串口获取文件内容功能尚未实现"))
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
