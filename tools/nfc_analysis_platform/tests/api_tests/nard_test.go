package api_tests

import (
	"net/http"
	"os"
	"path/filepath"
	"testing"

	"github.com/go-openapi/loads"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business/nard"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/tests/common"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestNardAPI(t *testing.T) {
	// 跳过测试，因为需要实现 API 处理程序
	// t.Skip("Skipping NARD API tests until API handlers are implemented")

	// Load Swagger spec
	swaggerSpec, err := loads.Analyzed(restapi.SwaggerJSON, "")
	require.NoError(t, err, "Failed to load Swagger spec")

	// Create API
	api := operations.NewNfcAnalysisPlatformAPI(swaggerSpec)

	// Register handlers
	// 注意：这些处理程序名称需要与 swagger 生成的代码匹配
	// 以下是示例，实际名称可能不同
	api.NardGetFormatTemplatesHandler = nard.NewGetFormatTemplatesHandler()
	api.NardGetFormatTemplateContentHandler = nard.NewGetFormatTemplateContentHandler()
	api.NardDecodeApduResponseHandler = nard.NewDecodeApduResponseHandler()
	api.NardGetFlipperDevicesHandler = nard.NewGetFlipperDevicesHandler()
	api.NardGetFlipperFilesHandler = nard.NewGetFlipperFilesHandler()
	api.NardGetFlipperFileContentHandler = nard.NewGetFlipperFileContentHandler()

	// Create test server
	server := common.APITestServer(t, api.Serve(nil))
	defer server.Close()

	t.Run("Get Format Templates", func(t *testing.T) {
		// Make request
		resp := common.MakeAPIRequest(t, server, "GET", "/api/nard/formats", nil)
		defer resp.Body.Close()

		// Check response
		assert.Equal(t, http.StatusOK, resp.StatusCode, "Response status code should be 200")

		// Parse response
		var apiResponse models.APIResponse
		common.ParseAPIResponse(t, resp.Body, &apiResponse)

		// Check response data
		assert.True(t, common.IsSuccessResponse(t, &apiResponse), "Response should be successful")
		assert.NotNil(t, apiResponse.Data, "Response data should not be nil")
	})

	t.Run("Get Format Template", func(t *testing.T) {
		// Make request - using "EMV" as a sample format ID
		resp := common.MakeAPIRequest(t, server, "GET", "/api/nard/formats/EMV", nil)
		defer resp.Body.Close()

		// Check response
		assert.Equal(t, http.StatusOK, resp.StatusCode, "Response status code should be 200")

		// Parse response
		var apiResponse models.APIResponse
		common.ParseAPIResponse(t, resp.Body, &apiResponse)

		// Check response data - this might be a success or error depending on if EMV format exists
		// Just check that we get a valid response structure
		assert.NotNil(t, apiResponse.Code, "Response should have a code")
		assert.NotNil(t, apiResponse.Message, "Response should have a message")
	})

	t.Run("Decode NARD Data", func(t *testing.T) {
		// 获取当前工作目录
		wd, err := os.Getwd()
		require.NoError(t, err, "Failed to get working directory")

		// 读取 emv.apdures 文件内容
		apduresPath := filepath.Join(wd, "nard_format", "emv.apdures")
		apduresData, err := os.ReadFile(apduresPath)
		require.NoError(t, err, "Failed to read emv.apdures file")

		// 读取 EMV.apdufmt 文件内容
		apdufmtPath := filepath.Join(wd, "nard_format", "EMV.apdufmt")
		apdufmtData, err := os.ReadFile(apdufmtPath)
		require.NoError(t, err, "Failed to read EMV.apdufmt file")

		// 将 apdures 文件内容转换为字符串
		responseData := string(apduresData)

		// 创建请求体
		debug := false
		requestBody := map[string]interface{}{
			"response_data":  responseData,
			"format_content": string(apdufmtData),
			"debug":          debug,
		}

		// 发送请求
		resp := common.MakeAPIRequest(t, server, "POST", "/api/nard/decode", requestBody)
		defer resp.Body.Close()

		// 检查响应状态码
		assert.Equal(t, http.StatusOK, resp.StatusCode, "Response status code should be 200")

		// 解析响应
		var apiResponse models.APIResponse
		common.ParseAPIResponse(t, resp.Body, &apiResponse)

		// 检查响应数据
		assert.NotNil(t, apiResponse.Code, "Response should have a code")
		assert.NotNil(t, apiResponse.Message, "Response should have a message")
	})

	t.Run("Get Flipper Devices", func(t *testing.T) {
		// Make request
		resp := common.MakeAPIRequest(t, server, "GET", "/api/nard/devices", nil)
		defer resp.Body.Close()

		// Check response
		assert.Equal(t, http.StatusOK, resp.StatusCode, "Response status code should be 200")

		// Parse response
		var apiResponse models.APIResponse
		common.ParseAPIResponse(t, resp.Body, &apiResponse)

		// Check response data
		assert.NotNil(t, apiResponse.Code, "Response should have a code")
		assert.NotNil(t, apiResponse.Message, "Response should have a message")
	})

	t.Run("Get Flipper Files", func(t *testing.T) {
		// Make request - this will likely fail in tests without a real device
		// but we can test the API structure
		resp := common.MakeAPIRequest(t, server, "GET", "/api/nard/files?device_path=/dev/ttyACM0", nil)
		defer resp.Body.Close()

		// Check response
		assert.Equal(t, http.StatusOK, resp.StatusCode, "Response status code should be 200")

		// Parse response
		var apiResponse models.APIResponse
		common.ParseAPIResponse(t, resp.Body, &apiResponse)

		// Check response data
		assert.NotNil(t, apiResponse.Code, "Response should have a code")
		assert.NotNil(t, apiResponse.Message, "Response should have a message")
	})
}
