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
		output, err := common.ExecuteCommand(t, binaryPath, "tlv", "parse", "--hex", common.SampleTLVData)
		require.NoError(t, err, "TLV command failed")
		assert.Contains(t, output, "6F", "Output should contain tag 6F")
		assert.Contains(t, output, "84", "Output should contain tag 84")
	})

	t.Run("TLV Command with Specific Tags", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "tlv", "extract", "--hex", common.SampleTLVData, "--tags", "84,50")
		require.NoError(t, err, "TLV command with specific tags failed")
		assert.Contains(t, output, "84:", "Output should contain tag 84")
		assert.Contains(t, output, "50:", "Output should contain tag 50")
	})

	t.Run("TLV Command with ASCII Type", func(t *testing.T) {
		output, err := common.ExecuteCommand(t, binaryPath, "tlv", "parse", "--hex", common.SampleTLVData, "--ascii")
		require.NoError(t, err, "TLV command with ASCII type failed")
		assert.Contains(t, output, "MasterCard", "Output should contain ASCII value")
	})

	t.Run("TLV Command with Invalid Hex", func(t *testing.T) {
		_, err := common.ExecuteCommand(t, binaryPath, "tlv", "parse", "--hex", "INVALID")
		assert.Error(t, err, "TLV command with invalid hex should fail")
	})
}
