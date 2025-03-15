/*
 * @Author: SpenserCai
 * @Date: 2025-03-15 16:54:15
 * @version:
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-15 18:21:39
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

	// 直接使用 testdata 目录中的文件
	testDataDir := filepath.Join(wd, "testdata")

	// 测试文件路径
	apduResFile := filepath.Join(testDataDir, "emv.apdures")
	formatFile := filepath.Join(testDataDir, "EMV.apdufmt")

	// 检查测试文件是否存在
	_, err = os.Stat(apduResFile)
	require.NoError(t, err, "Test file not found: %s", apduResFile)
	_, err = os.Stat(formatFile)
	require.NoError(t, err, "Test file not found: %s", formatFile)

	t.Run("Basic NARD Command", func(t *testing.T) {
		// 使用 --decode-format 参数来避免交互
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
		// 使用 ExecuteCommandSilent 来抑制错误输出
		_, err := common.ExecuteCommandSilent(t, binaryPath, "nard", "--file", "nonexistent.apdures")
		assert.Error(t, err, "NARD command with invalid file should fail")
		// 检查错误消息中是否包含 "exit status 1"
		assert.Contains(t, err.Error(), "exit status 1", "Error message should indicate command failure")
	})

	t.Run("NARD Command without Required Flags", func(t *testing.T) {
		// 使用 ExecuteCommandSilent 来抑制错误输出
		_, err := common.ExecuteCommandSilent(t, binaryPath, "nard")
		assert.Error(t, err, "NARD command without required flags should fail")
		// 检查错误消息中是否包含 "exit status 1"
		assert.Contains(t, err.Error(), "exit status 1", "Error message should indicate command failure")
	})
}
