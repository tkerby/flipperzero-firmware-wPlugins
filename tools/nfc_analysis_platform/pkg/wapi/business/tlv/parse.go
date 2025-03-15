package tlv

import (
	"github.com/go-openapi/runtime/middleware"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/tlv"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	tlvOps "github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations/tlv"
)

// 解析TLV数据处理器
func NewParseTlvDataHandler() tlvOps.ParseTlvDataHandler {
	return &parseTlvDataHandler{}
}

type parseTlvDataHandler struct{}

func (h *parseTlvDataHandler) Handle(params tlvOps.ParseTlvDataParams) middleware.Responder {
	if params.ParseRequest == nil {
		return tlvOps.NewParseTlvDataOK().WithPayload(business.ErrorResponse(400, "无效请求"))
	}

	// 解析TLV数据
	hexData := ""
	if params.ParseRequest.HexData != nil {
		hexData = *params.ParseRequest.HexData
	}

	if hexData == "" {
		return tlvOps.NewParseTlvDataOK().WithPayload(business.ErrorResponse(400, "十六进制数据为空"))
	}

	tlvList, err := tlv.ParseHex(hexData)
	if err != nil {
		return tlvOps.NewParseTlvDataOK().WithPayload(business.ErrorResponse(400, err.Error()))
	}

	// 转换为模型
	result := &models.TlvParseResult{
		Structure: convertTlvItems(tlvList),
	}

	return tlvOps.NewParseTlvDataOK().WithPayload(business.SuccessResponse(result))
}

// 转换TLV项
func convertTlvItems(tlvList tlv.TLVList) []*models.TlvItem {
	var result []*models.TlvItem
	for _, item := range tlvList {
		tlvItem := &models.TlvItem{
			Tag:           item.Tag.String(),
			Length:        int64(item.Length),
			Value:         item.Value.String(),
			IsConstructed: item.IsConstructed(),
		}

		if item.IsConstructed() {
			// 解析嵌套TLV
			nestedList, err := tlv.Parse(item.Value)
			if err == nil {
				tlvItem.Children = convertTlvItems(nestedList)
			}
		}

		result = append(result, tlvItem)
	}
	return result
}
