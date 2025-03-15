/*
 * @Author: SpenserCai
 * @Date: 2025-03-15 14:45:49
 * @version:
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-15 15:02:57
 * @Description: file content
 */
package business

import (
	"time"

	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
)

// 版本信息
var (
	Version   = "dev"
	BuildDate = time.Now().Format(time.RFC3339)
)

// 成功响应
func SuccessResponse(data interface{}) *models.APIResponse {
	return &models.APIResponse{
		Code:    0,
		Message: "success",
		Data:    data,
	}
}

// 错误响应
func ErrorResponse(code int32, message string) *models.APIResponse {
	return &models.APIResponse{
		Code:    code,
		Message: message,
		Data:    map[string]interface{}{},
	}
}
