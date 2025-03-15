/*
 * @Author: SpenserCai
 * @Date: 2025-03-15 16:54:32
 * @version:
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-15 17:48:52
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

func TestTlvCommand(t *testing.T) {
	// 获取项目根目录
	wd, err := os.Getwd()
	require.NoError(t, err, "Failed to get working directory")
	projectRoot := filepath.Dir(filepath.Dir(wd))
	binaryPath := filepath.Join(projectRoot, "nfc_analysis_platform")

	// 检查二进制文件是否存在
	if _, err := os.Stat(binaryPath); os.IsNotExist(err) {
		t.Skip("Binary not found, skipping test. Run 'make build' first.")
	}

	t.Run("Basic TLV Command", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "tlv", "--hex", common.SampleTLVData)
		require.NoError(t, err, "TLV command failed")
		assert.Contains(t, output, "TLV Data Parsing Results", "Output should contain title")
		assert.Contains(t, output, "Tag: 6f", "Output should contain tag 6F")
		assert.Contains(t, output, "Tag: 84", "Output should contain tag 84")
		assert.Contains(t, output, "Tag: a5", "Output should contain tag A5")
	})

	t.Run("TLV Command with Specific Tags", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "tlv", "--hex", common.SampleTLVData, "--tag", "84,50")
		require.NoError(t, err, "TLV command with specific tags failed")
		assert.Contains(t, output, "Tag: 84", "Output should contain tag 84")
		assert.Contains(t, output, "Tag: 50", "Output should contain tag 50")
	})

	t.Run("TLV Command with ASCII Type", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "tlv", "--hex", common.SampleTLVData, "--tag", "50", "--type", "ascii")
		require.NoError(t, err, "TLV command with ASCII type failed")
		assert.Contains(t, output, "MasterCard", "Output should contain ASCII value")
	})

	t.Run("TLV Command with Numeric Type", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "tlv", "--hex", common.SampleTLVData, "--type", "numeric")
		require.NoError(t, err, "TLV command with numeric type failed")
		assert.Contains(t, output, "TLV Data Parsing Results", "Output should contain title")
	})

	t.Run("TLV Command with Invalid Hex", func(t *testing.T) {
		// 使用 ExecuteCommand 函数，错误输出由命令行参数控制
		_, err := common.ExecuteCommand(t, binaryPath, "tlv", "--hex", "INVALID")
		assert.Error(t, err, "TLV command with invalid hex should fail")
		assert.Contains(t, err.Error(), "exit status 1", "Error message should indicate command failure")
	})
}
