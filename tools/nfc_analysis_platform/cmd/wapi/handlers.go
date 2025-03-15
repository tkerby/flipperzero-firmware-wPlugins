/*
 * @Author: SpenserCai
 * @Date: 2025-03-15 14:12:14
 * @version:
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-15 15:11:37
 * @Description: file content
 */
package wapi

import (
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business/nard"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business/system"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business/tlv"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations"
)

// 注册API处理器
func registerHandlers(api *operations.NfcAnalysisPlatformAPI) {
	// 系统相关处理器
	api.SystemGetSystemInfoHandler = system.NewGetSystemInfoHandler()

	// NARD相关处理器
	api.NardGetFormatTemplatesHandler = nard.NewGetFormatTemplatesHandler()
	api.NardGetFormatTemplateContentHandler = nard.NewGetFormatTemplateContentHandler()
	api.NardGetFlipperDevicesHandler = nard.NewGetFlipperDevicesHandler()
	api.NardGetFlipperFilesHandler = nard.NewGetFlipperFilesHandler()
	api.NardGetFlipperFileContentHandler = nard.NewGetFlipperFileContentHandler()
	api.NardDecodeApduResponseHandler = nard.NewDecodeApduResponseHandler()

	// TLV相关处理器
	api.TlvParseTlvDataHandler = tlv.NewParseTlvDataHandler()
	api.TlvExtractTlvValuesHandler = tlv.NewExtractTlvValuesHandler()
}
