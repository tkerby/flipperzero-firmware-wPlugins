// This file is safe to edit. Once it exists it will not be overwritten

package restapi

import (
	"crypto/tls"
	"net/http"

	"github.com/go-openapi/errors"
	"github.com/go-openapi/runtime"
	"github.com/go-openapi/runtime/middleware"

	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations/nard"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations/system"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations/tlv"
)

//go:generate swagger generate server --target ../../codegen --name NfcAnalysisPlatform --spec ../../swagger.yml --principal interface{} --exclude-main

func configureFlags(api *operations.NfcAnalysisPlatformAPI) {
	// api.CommandLineOptionsGroups = []swag.CommandLineOptionsGroup{ ... }
}

func configureAPI(api *operations.NfcAnalysisPlatformAPI) http.Handler {
	// configure the api here
	api.ServeError = errors.ServeError

	// Set your custom logger if needed. Default one is log.Printf
	// Expected interface func(string, ...interface{})
	//
	// Example:
	// api.Logger = log.Printf

	api.UseSwaggerUI()
	// To continue using redoc as your UI, uncomment the following line
	// api.UseRedoc()

	api.JSONConsumer = runtime.JSONConsumer()

	api.JSONProducer = runtime.JSONProducer()

	if api.NardDecodeApduResponseHandler == nil {
		api.NardDecodeApduResponseHandler = nard.DecodeApduResponseHandlerFunc(func(params nard.DecodeApduResponseParams) middleware.Responder {
			return middleware.NotImplemented("operation nard.DecodeApduResponse has not yet been implemented")
		})
	}
	if api.TlvExtractTlvValuesHandler == nil {
		api.TlvExtractTlvValuesHandler = tlv.ExtractTlvValuesHandlerFunc(func(params tlv.ExtractTlvValuesParams) middleware.Responder {
			return middleware.NotImplemented("operation tlv.ExtractTlvValues has not yet been implemented")
		})
	}
	if api.NardGetFlipperDevicesHandler == nil {
		api.NardGetFlipperDevicesHandler = nard.GetFlipperDevicesHandlerFunc(func(params nard.GetFlipperDevicesParams) middleware.Responder {
			return middleware.NotImplemented("operation nard.GetFlipperDevices has not yet been implemented")
		})
	}
	if api.NardGetFlipperFileContentHandler == nil {
		api.NardGetFlipperFileContentHandler = nard.GetFlipperFileContentHandlerFunc(func(params nard.GetFlipperFileContentParams) middleware.Responder {
			return middleware.NotImplemented("operation nard.GetFlipperFileContent has not yet been implemented")
		})
	}
	if api.NardGetFlipperFilesHandler == nil {
		api.NardGetFlipperFilesHandler = nard.GetFlipperFilesHandlerFunc(func(params nard.GetFlipperFilesParams) middleware.Responder {
			return middleware.NotImplemented("operation nard.GetFlipperFiles has not yet been implemented")
		})
	}
	if api.NardGetFormatTemplateContentHandler == nil {
		api.NardGetFormatTemplateContentHandler = nard.GetFormatTemplateContentHandlerFunc(func(params nard.GetFormatTemplateContentParams) middleware.Responder {
			return middleware.NotImplemented("operation nard.GetFormatTemplateContent has not yet been implemented")
		})
	}
	if api.NardGetFormatTemplatesHandler == nil {
		api.NardGetFormatTemplatesHandler = nard.GetFormatTemplatesHandlerFunc(func(params nard.GetFormatTemplatesParams) middleware.Responder {
			return middleware.NotImplemented("operation nard.GetFormatTemplates has not yet been implemented")
		})
	}
	if api.SystemGetSystemInfoHandler == nil {
		api.SystemGetSystemInfoHandler = system.GetSystemInfoHandlerFunc(func(params system.GetSystemInfoParams) middleware.Responder {
			return middleware.NotImplemented("operation system.GetSystemInfo has not yet been implemented")
		})
	}
	if api.TlvParseTlvDataHandler == nil {
		api.TlvParseTlvDataHandler = tlv.ParseTlvDataHandlerFunc(func(params tlv.ParseTlvDataParams) middleware.Responder {
			return middleware.NotImplemented("operation tlv.ParseTlvData has not yet been implemented")
		})
	}

	api.PreServerShutdown = func() {}

	api.ServerShutdown = func() {}

	return setupGlobalMiddleware(api.Serve(setupMiddlewares))
}

// The TLS configuration before HTTPS server starts.
func configureTLS(tlsConfig *tls.Config) {
	// Make all necessary changes to the TLS configuration here.
}

// As soon as server is initialized but not run yet, this function will be called.
// If you need to modify a config, store server instance to stop it individually later, this is the place.
// This function can be called multiple times, depending on the number of serving schemes.
// scheme value will be set accordingly: "http", "https" or "unix".
func configureServer(s *http.Server, scheme, addr string) {
}

// The middleware configuration is for the handler executors. These do not apply to the swagger.json document.
// The middleware executes after routing but before authentication, binding and validation.
func setupMiddlewares(handler http.Handler) http.Handler {
	return handler
}

// The middleware configuration happens before anything, this middleware also applies to serving the swagger.json document.
// So this is a good place to plug in a panic handling middleware, logging and metrics.
func setupGlobalMiddleware(handler http.Handler) http.Handler {
	return handler
}
