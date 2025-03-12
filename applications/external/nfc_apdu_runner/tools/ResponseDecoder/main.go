/*
 * @Author: SpenserCai
 * @Date: 2025-03-11 16:04:23
 * @version:
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-12 18:22:26
 * @Description: file content
 */
package main

import (
	"fmt"
	"os"

	"github.com/spensercai/nfc_apdu_runner/tools/ResponseDecoder/cmd"
)

func main() {
	if err := cmd.Execute(); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %s\n", err)
		os.Exit(1)
	}
}
