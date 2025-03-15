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

	// 创建测试数据目录
	testDataDir := filepath.Join(t.TempDir(), "testdata")
	err = os.MkdirAll(testDataDir, 0755)
	require.NoError(t, err, "Failed to create test data directory")

	// 创建测试数据文件
	err = common.CreateTestDataFiles(testDataDir)
	require.NoError(t, err, "Failed to create test data files")

	// 测试文件路径
	apduResFile := filepath.Join(testDataDir, "sample.apdures")
	formatFile := filepath.Join(testDataDir, "sample.apdufmt")

	t.Run("Basic NARD Command", func(t *testing.T) {
		// 由于 nard 命令需要用户交互选择格式文件，这个测试可能会失败
		// 我们可以通过提供 --decode-format 参数来避免交互
		output, err := common.ExecuteCommand(t, binaryPath, "nard", "--file", apduResFile, "--decode-format", formatFile)
		require.NoError(t, err, "NARD command failed")
		assert.Contains(t, output, "EMV Card Information", "Output should contain format header")
		assert.Contains(t, output, "Application Label:", "Output should contain formatted data")
	})

	t.Run("NARD Command with Debug", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "nard", "--file", apduResFile, "--decode-format", formatFile, "--debug")
		require.NoError(t, err, "NARD command with debug failed")
		// 在调试模式下，输出可能包含更多信息
		assert.Contains(t, output, "EMV Card Information", "Output should contain format header")
	})

	t.Run("NARD Command with Invalid File", func(t *testing.T) {
		_, err := common.ExecuteCommand(t, binaryPath, "nard", "--file", "nonexistent.apdures")
		assert.Error(t, err, "NARD command with invalid file should fail")
		assert.Contains(t, err.Error(), "failed to read file", "Error message should mention file reading failure")
	})

	t.Run("NARD Command without Required Flags", func(t *testing.T) {
		_, err := common.ExecuteCommand(t, binaryPath, "nard")
		assert.Error(t, err, "NARD command without required flags should fail")
		assert.Contains(t, err.Error(), "either --file or --device flag must be specified", "Error message should mention missing flags")
	})
}
