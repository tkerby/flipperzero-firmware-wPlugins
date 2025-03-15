package nard

import (
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"

	"github.com/go-openapi/runtime/middleware"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/business"
	"github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/models"
	nardOps "github.com/spensercai/nfc_apdu_runner/tools/nfc_analysis_platform/pkg/wapi/codegen/restapi/operations/nard"
)

// 默认格式模板目录
const defaultFormatDir = "nard_format"

// 获取格式模板列表处理器
func NewGetFormatTemplatesHandler() nardOps.GetFormatTemplatesHandler {
	return &getFormatTemplatesHandler{}
}

type getFormatTemplatesHandler struct{}

func (h *getFormatTemplatesHandler) Handle(params nardOps.GetFormatTemplatesParams) middleware.Responder {
	directory := defaultFormatDir
	if params.Directory != nil && *params.Directory != "" {
		directory = *params.Directory
	}

	formats, err := listFormatFiles(directory)
	if err != nil {
		return nardOps.NewGetFormatTemplatesOK().WithPayload(business.ErrorResponse(404, err.Error()))
	}

	var result []*models.FormatTemplate
	for _, format := range formats {
		// 从文件中读取描述信息
		description := ""
		content, err := readFormatFile(filepath.Join(directory, format))
		if err == nil {
			// 尝试从内容中提取描述
			lines := strings.Split(content, "\n")
			for _, line := range lines {
				if strings.HasPrefix(line, "# Description:") {
					description = strings.TrimSpace(strings.TrimPrefix(line, "# Description:"))
					break
				}
			}
		}

		result = append(result, &models.FormatTemplate{
			ID:          format,
			Name:        strings.TrimSuffix(format, filepath.Ext(format)),
			Path:        filepath.Join(directory, format),
			Description: description,
		})
	}

	return nardOps.NewGetFormatTemplatesOK().WithPayload(business.SuccessResponse(result))
}

// 获取格式模板内容处理器
func NewGetFormatTemplateContentHandler() nardOps.GetFormatTemplateContentHandler {
	return &getFormatTemplateContentHandler{}
}

type getFormatTemplateContentHandler struct{}

func (h *getFormatTemplateContentHandler) Handle(params nardOps.GetFormatTemplateContentParams) middleware.Responder {
	directory := defaultFormatDir
	if params.Directory != nil && *params.Directory != "" {
		directory = *params.Directory
	}

	formatID := params.FormatID
	path := filepath.Join(directory, formatID)

	// 读取格式模板文件
	data, err := readFormatFile(path)
	if err != nil {
		return nardOps.NewGetFormatTemplateContentOK().WithPayload(business.ErrorResponse(404, err.Error()))
	}

	result := &models.FormatTemplateContent{
		ID:      formatID,
		Name:    strings.TrimSuffix(formatID, filepath.Ext(formatID)),
		Content: data,
	}

	return nardOps.NewGetFormatTemplateContentOK().WithPayload(business.SuccessResponse(result))
}

// 列出格式文件
func listFormatFiles(directory string) ([]string, error) {
	// 确保目录存在
	if _, err := os.Stat(directory); os.IsNotExist(err) {
		if err := os.MkdirAll(directory, 0755); err != nil {
			return nil, err
		}
	}

	// 读取目录内容
	files, err := ioutil.ReadDir(directory)
	if err != nil {
		return nil, err
	}

	// 过滤出.apdufmt文件
	var formatFiles []string
	for _, file := range files {
		if !file.IsDir() && strings.HasSuffix(file.Name(), ".apdufmt") {
			formatFiles = append(formatFiles, file.Name())
		}
	}

	return formatFiles, nil
}

// 读取格式文件内容
func readFormatFile(path string) (string, error) {
	data, err := ioutil.ReadFile(path)
	if err != nil {
		return "", err
	}
	return string(data), nil
}
