package api_tests

import (
	"net/http"
	"testing"

	"github.com/go-openapi/loads"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business/tlv"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/tests/common"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestTlvAPI(t *testing.T) {
	// 跳过测试，因为需要实现 API 处理程序
	// t.Skip("Skipping TLV API tests until API handlers are implemented")

	// Load Swagger spec
	swaggerSpec, err := loads.Analyzed(restapi.SwaggerJSON, "")
	require.NoError(t, err, "Failed to load Swagger spec")

	// Create API
	api := operations.NewNfcAnalysisPlatformAPI(swaggerSpec)

	// Register handlers
	api.TlvParseTlvDataHandler = tlv.NewParseTlvDataHandler()
	api.TlvExtractTlvValuesHandler = tlv.NewExtractTlvValuesHandler()

	// Create test server
	server := common.APITestServer(t, api.Serve(nil))
	defer server.Close()

	// Sample TLV data
	tlvData := common.SampleTLVData

	// @TestInfo: TLV解析|解析TLV数据|测试解析十六进制格式的TLV数据结构
	t.Run("Parse TLV Data", func(t *testing.T) {
		// Create request body
		hexData := tlvData
		requestBody := map[string]interface{}{
			"hex_data": hexData,
		}

		// Make request
		resp := common.MakeAPIRequest(t, server, "POST", "/api/tlv/parse", requestBody)
		defer resp.Body.Close()

		// Check response
		assert.Equal(t, http.StatusOK, resp.StatusCode, "Response status code should be 200")

		// Parse response
		var apiResponse models.APIResponse
		common.ParseAPIResponse(t, resp.Body, &apiResponse)

		// 输出JSON格式的响应
		common.LogAPIResponseJSON(t, apiResponse)

		// Check response data
		assert.True(t, common.IsSuccessResponse(t, &apiResponse), "Response should be successful")
		assert.NotNil(t, apiResponse.Data, "Response data should not be nil")

		// Check TLV parse result
		result, ok := apiResponse.Data.(map[string]interface{})
		assert.True(t, ok, "Response data should be a map")
		assert.Contains(t, result, "structure", "Result should contain structure")
	})

	// @TestInfo: TLV提取|提取TLV特定标签值|测试从TLV数据中提取指定标签的值并转换为ASCII格式
	t.Run("Extract TLV Values", func(t *testing.T) {
		// Create request body
		requestBody := map[string]interface{}{
			"hex_data":  tlvData,
			"tags":      []string{"84", "50"},
			"data_type": "ascii",
		}

		// Make request
		resp := common.MakeAPIRequest(t, server, "POST", "/api/tlv/extract", requestBody)
		defer resp.Body.Close()

		// Check response
		assert.Equal(t, http.StatusOK, resp.StatusCode, "Response status code should be 200")

		// Parse response
		var apiResponse models.APIResponse
		common.ParseAPIResponse(t, resp.Body, &apiResponse)

		// 输出JSON格式的响应
		common.LogAPIResponseJSON(t, apiResponse)

		// Check response data
		assert.True(t, common.IsSuccessResponse(t, &apiResponse), "Response should be successful")
		assert.NotNil(t, apiResponse.Data, "Response data should not be nil")

		// Check TLV extract result
		result, ok := apiResponse.Data.(map[string]interface{})
		assert.True(t, ok, "Response data should be a map")
		assert.Contains(t, result, "values", "Result should contain values")
	})

	// @TestInfo: TLV错误处理|处理无效的十六进制数据|测试当提供无效的十六进制数据时的错误处理
	t.Run("Parse TLV Data with Invalid Hex", func(t *testing.T) {
		// Create request body
		requestBody := map[string]interface{}{
			"hex_data": "INVALID",
		}

		// Make request
		resp := common.MakeAPIRequest(t, server, "POST", "/api/tlv/parse", requestBody)
		defer resp.Body.Close()

		// Check response
		assert.Equal(t, http.StatusOK, resp.StatusCode, "Response status code should be 200")

		// Parse response
		var apiResponse models.APIResponse
		common.ParseAPIResponse(t, resp.Body, &apiResponse)

		// 输出JSON格式的响应
		common.LogAPIResponseJSON(t, apiResponse)

		// Check response data
		assert.NotEqual(t, 0, apiResponse.Code, "Response should have non-zero code")
		assert.NotEqual(t, "success", apiResponse.Message, "Response should have error message")
	})
}
