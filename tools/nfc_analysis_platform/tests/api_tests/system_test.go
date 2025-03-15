package api_tests

import (
	"net/http"
	"testing"

	"github.com/go-openapi/loads"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business/system"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/tests/common"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestSystemInfoAPI(t *testing.T) {
	// Load Swagger spec
	swaggerSpec, err := loads.Analyzed(restapi.SwaggerJSON, "")
	require.NoError(t, err, "Failed to load Swagger spec")

	// Create API
	api := operations.NewNfcAnalysisPlatformAPI(swaggerSpec)

	// Register handlers
	api.SystemGetSystemInfoHandler = system.NewGetSystemInfoHandler()

	// Create test server
	server := common.APITestServer(t, api.Serve(nil))
	defer server.Close()

	t.Run("Get System Info", func(t *testing.T) {
		// Make request
		resp := common.MakeAPIRequest(t, server, "GET", "/api/system/info", nil)
		defer resp.Body.Close()

		// Check response
		assert.Equal(t, http.StatusOK, resp.StatusCode, "Response status code should be 200")

		// Parse response
		var apiResponse models.APIResponse
		common.ParseAPIResponse(t, resp.Body, &apiResponse)

		// Check response data
		assert.True(t, common.IsSuccessResponse(t, &apiResponse), "Response should be successful")
		assert.NotNil(t, apiResponse.Data, "Response data should not be nil")

		// Check system info fields
		systemInfo, ok := apiResponse.Data.(map[string]interface{})
		assert.True(t, ok, "Response data should be a map")
		assert.Contains(t, systemInfo, "version", "System info should contain version")
		assert.Contains(t, systemInfo, "build_date", "System info should contain build date")
		assert.Contains(t, systemInfo, "go_version", "System info should contain Go version")
		assert.Contains(t, systemInfo, "os", "System info should contain OS")
		assert.Contains(t, systemInfo, "arch", "System info should contain architecture")
	})
}
