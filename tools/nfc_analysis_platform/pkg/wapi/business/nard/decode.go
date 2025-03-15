package nard

import (
	"path/filepath"

	"github.com/go-openapi/runtime/middleware"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/decoder"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	nardOps "github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations/nard"
)

// 解码APDU响应处理器
func NewDecodeApduResponseHandler() nardOps.DecodeApduResponseHandler {
	return &decodeApduResponseHandler{}
}

type decodeApduResponseHandler struct{}

func (h *decodeApduResponseHandler) Handle(params nardOps.DecodeApduResponseParams) middleware.Responder {
	if params.DecodeRequest == nil {
		return nardOps.NewDecodeApduResponseOK().WithPayload(business.ErrorResponse(400, "无效请求"))
	}

	// 解析响应数据
	responseData := ""
	if params.DecodeRequest.ResponseData != nil {
		responseData = *params.DecodeRequest.ResponseData
	}

	// 检查响应数据
	if responseData == "" {
		return nardOps.NewDecodeApduResponseOK().WithPayload(business.ErrorResponse(400, "响应数据为空"))
	}

	// 解析响应数据
	response, err := decoder.ParseResponseData(responseData)
	if err != nil {
		return nardOps.NewDecodeApduResponseOK().WithPayload(business.ErrorResponse(400, "解析响应数据失败: "+err.Error()))
	}

	// 获取格式模板内容
	var formatContent string

	// 从格式ID获取内容
	if params.DecodeRequest.FormatID != "" {
		formatID := params.DecodeRequest.FormatID
		directory := defaultFormatDir

		path := filepath.Join(directory, formatID)
		content, err := readFormatFile(path)
		if err != nil {
			return nardOps.NewDecodeApduResponseOK().WithPayload(business.ErrorResponse(404, "读取格式模板失败: "+err.Error()))
		}
		formatContent = content
	} else if params.DecodeRequest.FormatContent != "" {
		// 直接使用提供的格式内容
		formatContent = params.DecodeRequest.FormatContent
	} else {
		return nardOps.NewDecodeApduResponseOK().WithPayload(business.ErrorResponse(400, "未提供格式模板ID或内容"))
	}

	// 是否启用调试模式
	debug := false
	if params.DecodeRequest.Debug != nil {
		debug = *params.DecodeRequest.Debug
	}

	// 创建解码上下文
	ctx := decoder.NewDecodeContext(response, debug)

	// 解码响应
	decodedOutput, err := decoder.DecodeResponse(response, formatContent, debug)
	if err != nil {
		return nardOps.NewDecodeApduResponseOK().WithPayload(business.ErrorResponse(500, "解码失败: "+err.Error()))
	}

	// 构建结果
	var errors []string
	if debug && len(ctx.Errors) > 0 {
		// 在调试模式下收集错误信息
		for _, errInfo := range ctx.Errors {
			errors = append(errors, errInfo.Description)
		}
	}

	// 获取状态码
	statusCode := ""
	if len(response.Outputs) > 0 && len(response.Outputs[0]) >= 4 {
		// 假设状态码在第一行输出的最后4个字符
		statusCode = response.Outputs[0][len(response.Outputs[0])-4:]
	}

	result := &models.DecodeResult{
		Status: statusCode,
		Output: decodedOutput,
		Errors: errors,
	}

	return nardOps.NewDecodeApduResponseOK().WithPayload(business.SuccessResponse(result))
}
