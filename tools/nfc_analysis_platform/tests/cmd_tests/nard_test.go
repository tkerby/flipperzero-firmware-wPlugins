/*
 * @Author: SpenserCai
 * @Date: 2025-03-15 16:54:15
 * @version:
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-15 17:33:48
 * @Description: file content
 */
package cmd_tests

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/tests/common"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestNardCommand(t *testing.T) {

	// 获取项目根目录
	wd, err := os.Getwd()
	require.NoError(t, err, "Failed to get working directory")
	projectRoot := filepath.Dir(filepath.Dir(wd))
	binaryPath := filepath.Join(projectRoot, "nfc_analysis_platform")

	// 检查二进制文件是否存在
	if _, err := os.Stat(binaryPath); os.IsNotExist(err) {
		t.Skip("Binary not found, skipping test. Run 'make build' first.")
	}

	// Create test data directory
	testDataDir := filepath.Join(t.TempDir(), "testdata")
	err = os.MkdirAll(testDataDir, 0755)
	require.NoError(t, err, "Failed to create test data directory")

	// Create test data files
	err = common.CreateTestDataFiles(testDataDir)
	require.NoError(t, err, "Failed to create test data files")

	// Test file path
	apduResFile := filepath.Join(testDataDir, "sample.apdures")
	formatFile := filepath.Join(testDataDir, "sample.apdufmt")

	t.Run("Basic NARD Command", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "nard", "--file", apduResFile)
		require.NoError(t, err, "NARD command failed")
		assert.Contains(t, output, "Flipper NFC APDU Runner", "Output should contain header")
		assert.Contains(t, output, "UID: 04A23B9C5D7E8F", "Output should contain UID")
	})

	t.Run("NARD Command with Format", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "nard", "--file", apduResFile, "--decode-format", formatFile)
		require.NoError(t, err, "NARD command with format failed")
		assert.Contains(t, output, "EMV Card Information", "Output should contain format header")
		assert.Contains(t, output, "Application Label:", "Output should contain formatted data")
	})

	t.Run("NARD Command with Debug", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "nard", "--file", apduResFile, "--debug")
		require.NoError(t, err, "NARD command with debug failed")
		assert.Contains(t, output, "Debug Information:", "Output should contain debug information")
	})

	t.Run("NARD Command with Invalid File", func(t *testing.T) {
		_, err := common.ExecuteCommand(t, binaryPath, "nard", "--file", "nonexistent.apdures")
		assert.Error(t, err, "NARD command with invalid file should fail")
	})
}
