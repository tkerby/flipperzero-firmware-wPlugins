package system

import (
	"runtime"
	"time"

	"github.com/go-openapi/runtime/middleware"
	"github.com/go-openapi/strfmt"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	systemOps "github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations/system"
)

// 获取系统信息处理器
func NewGetSystemInfoHandler() systemOps.GetSystemInfoHandler {
	return &getSystemInfoHandler{}
}

type getSystemInfoHandler struct{}

func (h *getSystemInfoHandler) Handle(params systemOps.GetSystemInfoParams) middleware.Responder {
	// 解析构建日期
	buildTime, err := time.Parse(time.RFC3339, business.BuildDate)
	if err != nil {
		buildTime = time.Now() // 如果解析失败，使用当前时间
	}

	// 获取系统信息
	info := &models.SystemInfo{
		Version:   business.Version,
		BuildDate: strfmt.DateTime(buildTime),
		GoVersion: runtime.Version(),
		Os:        runtime.GOOS,
		Arch:      runtime.GOARCH,
	}

	return systemOps.NewGetSystemInfoOK().WithPayload(business.SuccessResponse(info))
}
