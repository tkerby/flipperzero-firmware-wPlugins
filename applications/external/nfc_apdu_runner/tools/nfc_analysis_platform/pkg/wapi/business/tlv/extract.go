package tlv

import (
	"github.com/go-openapi/runtime/middleware"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/tlv"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	tlvOps "github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations/tlv"
)

// 提取TLV值处理器
func NewExtractTlvValuesHandler() tlvOps.ExtractTlvValuesHandler {
	return &extractTlvValuesHandler{}
}

type extractTlvValuesHandler struct{}

func (h *extractTlvValuesHandler) Handle(params tlvOps.ExtractTlvValuesParams) middleware.Responder {
	if params.ExtractRequest == nil {
		return tlvOps.NewExtractTlvValuesOK().WithPayload(business.ErrorResponse(400, "无效请求"))
	}

	// 获取请求参数
	hexData := ""
	if params.ExtractRequest.HexData != nil {
		hexData = *params.ExtractRequest.HexData
	}

	if hexData == "" {
		return tlvOps.NewExtractTlvValuesOK().WithPayload(business.ErrorResponse(400, "十六进制数据为空"))
	}

	// 获取要提取的标签
	var tags []string
	if params.ExtractRequest.Tags != nil {
		tags = params.ExtractRequest.Tags
	}

	if len(tags) == 0 {
		return tlvOps.NewExtractTlvValuesOK().WithPayload(business.ErrorResponse(400, "未指定要提取的标签"))
	}

	// 获取数据类型
	dataType := "hex"
	if params.ExtractRequest.DataType != nil {
		dataType = *params.ExtractRequest.DataType
	}

	// 提取值
	values := make(map[string]string)
	for _, tag := range tags {
		var value string
		var err error

		switch dataType {
		case "utf8", "ascii":
			value, err = tlv.GetTagValueAsString(hexData, tag, dataType)
		case "numeric":
			var intValue int
			intValue, err = tlv.GetTagValueAsInt(hexData, tag)
			if err == nil {
				value = string(intValue)
			}
		default: // hex
			value, err = tlv.GetTagValue(hexData, tag)
		}

		if err == nil {
			values[tag] = value
		}
	}

	// 构建结果
	result := &models.TlvExtractResult{
		Values: values,
	}

	return tlvOps.NewExtractTlvValuesOK().WithPayload(business.SuccessResponse(result))
}
