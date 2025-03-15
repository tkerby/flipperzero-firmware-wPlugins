/*
 * @Author: SpenserCai
 * @Date: 2025-03-15 14:12:01
 * @version:
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-15 15:25:36
 * @Description: file content
 */
package wapi

import (
	"context"
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/go-openapi/loads"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations"
	"github.com/spf13/cobra"
)

var (
	host string
	port int
)

// WapiCmd represents the wapi command
var WapiCmd = &cobra.Command{
	Use:   "wapi",
	Short: "Start Web API service",
	Long: `Start the Web API service for NFC Analysis Platform,
providing system information, NARD format template management, 
Flipper device interaction and TLV parsing functionality.`,
	RunE: func(cmd *cobra.Command, args []string) error {
		// Create a context for handling cancellation signals
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		// Set up signal handling
		go handleSignals(cancel)

		// Start the server
		address := fmt.Sprintf("%s:%d", host, port)
		fmt.Printf("Starting Web API server, listening on: %s\n", address)

		// Load Swagger specification
		swaggerSpec, err := loads.Analyzed(restapi.SwaggerJSON, "")
		if err != nil {
			return fmt.Errorf("failed to load Swagger specification: %w", err)
		}

		api := operations.NewNfcAnalysisPlatformAPI(swaggerSpec)

		// Register handlers
		registerHandlers(api)

		server := restapi.NewServer(api)
		defer server.Shutdown()

		server.Host = host
		server.Port = port

		// 只启用HTTP，禁用HTTPS
		server.EnabledListeners = []string{"http"}

		// Configure the server
		server.ConfigureAPI()

		// Start the server (run in a goroutine to handle signals)
		go func() {
			if err := server.Serve(); err != nil {
				log.Printf("Server error: %v", err)
				cancel()
			}
		}()

		// Wait for context cancellation
		<-ctx.Done()
		return nil
	},
}

// Handle termination signals
func handleSignals(cancel context.CancelFunc) {
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan
	log.Println("Received termination signal, shutting down server...")
	cancel()
}

func init() {
	WapiCmd.Flags().StringVar(&host, "host", "127.0.0.1", "API server host address")
	WapiCmd.Flags().IntVar(&port, "port", 8280, "API server port")
}
