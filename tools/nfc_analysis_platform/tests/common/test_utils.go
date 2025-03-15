package common

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"

	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	"github.com/stretchr/testify/require"
)

// ExecuteCommand executes a command and returns its output
func ExecuteCommand(t *testing.T, command string, args ...string) (string, error) {
	cmd := exec.Command(command, args...)
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	err := cmd.Run()
	if err != nil {
		t.Logf("Command stderr: %s", stderr.String())
		return stdout.String(), err
	}
	return stdout.String(), nil
}

// GetTestDataPath returns the absolute path to a test data file
func GetTestDataPath(t *testing.T, relativePath string) string {
	// Get the absolute path to the project root
	wd, err := os.Getwd()
	require.NoError(t, err, "Failed to get working directory")

	// Navigate to the test data directory
	testDataPath := filepath.Join(wd, relativePath)
	_, err = os.Stat(testDataPath)
	require.NoError(t, err, "Test data file not found: %s", testDataPath)

	return testDataPath
}

// CreateTempFile creates a temporary file with the given content
func CreateTempFile(t *testing.T, content string, prefix string) string {
	tmpfile, err := os.CreateTemp("", prefix)
	require.NoError(t, err, "Failed to create temporary file")

	_, err = tmpfile.Write([]byte(content))
	require.NoError(t, err, "Failed to write to temporary file")

	err = tmpfile.Close()
	require.NoError(t, err, "Failed to close temporary file")

	return tmpfile.Name()
}

// CleanupTempFile removes a temporary file
func CleanupTempFile(t *testing.T, path string) {
	err := os.Remove(path)
	require.NoError(t, err, "Failed to remove temporary file")
}

// APITestServer creates a test server for API testing
func APITestServer(t *testing.T, handler http.Handler) *httptest.Server {
	return httptest.NewServer(handler)
}

// ParseAPIResponse parses an API response into the given model
func ParseAPIResponse(t *testing.T, body io.Reader, model interface{}) {
	data, err := io.ReadAll(body)
	require.NoError(t, err, "Failed to read response body")

	err = json.Unmarshal(data, model)
	require.NoError(t, err, "Failed to unmarshal response body")
}

// IsSuccessResponse checks if an API response is successful
func IsSuccessResponse(t *testing.T, response *models.APIResponse) bool {
	return response.Code == 0 && strings.ToLower(response.Message) == "success"
}

// MakeAPIRequest makes an API request and returns the response
func MakeAPIRequest(t *testing.T, server *httptest.Server, method, path string, body interface{}) *http.Response {
	var reqBody io.Reader
	if body != nil {
		jsonBody, err := json.Marshal(body)
		require.NoError(t, err, "Failed to marshal request body")
		reqBody = bytes.NewBuffer(jsonBody)
	}

	req, err := http.NewRequest(method, server.URL+path, reqBody)
	require.NoError(t, err, "Failed to create request")
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{}
	resp, err := client.Do(req)
	require.NoError(t, err, "Failed to make request")

	return resp
}
