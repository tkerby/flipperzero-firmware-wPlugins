/*
 * @Author: SpenserCai
 * @Date: 2025-03-14 18:03:48
 * @version:
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-15 14:16:27
 * @Description: file content
 */
package cmd

import (
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/cmd/nard"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/cmd/tlv"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/cmd/wapi"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/cmd/wapp"
	"github.com/spf13/cobra"
)

// rootCmd represents the base command when called without any subcommands
var rootCmd = &cobra.Command{
	Use:   "nfc_analysis_platform",
	Short: "NFC Analysis Platform for Flipper Zero",
	Long: `NFC Analysis Platform is a comprehensive tool for analyzing NFC data
from Flipper Zero NFC applications.

This platform provides various tools for NFC data analysis, including:
- APDU Response Decoder (nard)
- TLV Parser (tlv)
- Web API Server (wapi)
`,
}

// Execute adds all child commands to the root command and sets flags appropriately.
// This is called by main.main(). It only needs to happen once to the rootCmd.
func Execute() error {
	return rootCmd.Execute()
}

func init() {
	// 禁用completion命令
	rootCmd.CompletionOptions.DisableDefaultCmd = true

	// 添加子命令
	rootCmd.AddCommand(nard.NardCmd)
	rootCmd.AddCommand(tlv.TlvCmd)
	rootCmd.AddCommand(wapi.WapiCmd)
	rootCmd.AddCommand(wapp.WappCmd)
}
