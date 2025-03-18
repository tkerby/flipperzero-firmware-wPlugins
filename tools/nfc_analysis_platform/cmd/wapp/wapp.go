/*
 * @Author: SpenserCai
 * @Date: 2024-04-22
 * @version:
 * @LastEditors: SpenserCai
 * @LastEditTime: 2024-04-22
 * @Description: WAPP command - Web App Server
 */
package wapp

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"net/http/httputil"
	"net/url"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"github.com/spf13/cobra"
)

var (
	host   string // Web服务器主机地址
	port   int    // Web服务器端口
	s_host string // API服务器主机地址
	s_port int    // API服务器端口
	webDir string // Web目录路径
)

// WappCmd represents the wapp command
var WappCmd = &cobra.Command{
	Use:   "wapp",
	Short: "Start Web App service",
	Long: `Start the Web App service for NFC Analysis Platform,
serving the web UI and proxying API requests to the API server.
This command starts both the web server and API server.`,
	RunE: func(cmd *cobra.Command, args []string) error {
		// 创建上下文用于处理取消信号
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		// 设置信号处理
		go handleSignals(cancel)

		// 检查web目录是否存在
		absWebDir, err := filepath.Abs(webDir)
		if err != nil {
			return fmt.Errorf("failed to get absolute path for web directory: %w", err)
		}

		if _, err := os.Stat(absWebDir); os.IsNotExist(err) {
			return fmt.Errorf("web directory does not exist: %s", absWebDir)
		}

		// 启动API服务器
		apiServerDone := make(chan struct{})
		apiUrl := fmt.Sprintf("http://%s:%d", s_host, s_port)
		fmt.Printf("Starting API server at %s\n", apiUrl)

		go func() {
			defer close(apiServerDone)

			// 构建wapi命令
			wapiCmd := exec.CommandContext(ctx, os.Args[0], "wapi",
				"--host", s_host,
				"--port", fmt.Sprintf("%d", s_port))

			// 设置标准输出和标准错误
			wapiCmd.Stdout = os.Stdout
			wapiCmd.Stderr = os.Stderr

			// 启动wapi命令
			if err := wapiCmd.Start(); err != nil {
				log.Printf("Failed to start API server: %v", err)
				cancel()
				return
			}

			// 等待API服务器启动
			time.Sleep(1 * time.Second)

			// 等待命令完成
			if err := wapiCmd.Wait(); err != nil {
				if ctx.Err() == nil { // 如果不是因为上下文取消
					log.Printf("API server exited with error: %v", err)
					cancel()
				}
			}
		}()

		// 创建API反向代理
		apiTargetUrl, err := url.Parse(apiUrl)
		if err != nil {
			return fmt.Errorf("failed to parse API URL: %w", err)
		}

		apiProxy := httputil.NewSingleHostReverseProxy(apiTargetUrl)

		// 创建Web服务器
		webAddress := fmt.Sprintf("%s:%d", host, port)
		fmt.Printf("Starting Web server at http://%s\n", webAddress)

		// 创建HTTP服务器
		server := &http.Server{
			Addr: webAddress,
			Handler: http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				// 检查是否是API请求
				if strings.HasPrefix(r.URL.Path, "/api/") {
					// 转发到API服务器
					apiProxy.ServeHTTP(w, r)
					return
				}

				// 检查请求路径是否存在对应的文件
				requestedPath := filepath.Join(absWebDir, r.URL.Path)
				fileInfo, err := os.Stat(requestedPath)

				if os.IsNotExist(err) || fileInfo.IsDir() {
					// 如果文件不存在或是目录，则返回index.html
					http.ServeFile(w, r, filepath.Join(absWebDir, "index.html"))
					return
				}

				// 提供静态文件
				http.FileServer(http.Dir(absWebDir)).ServeHTTP(w, r)
			}),
		}

		// 在goroutine中启动服务器
		go func() {
			if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
				log.Printf("Web server error: %v", err)
				cancel()
			}
		}()

		// 等待上下文取消
		<-ctx.Done()

		// 优雅关闭Web服务器
		shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer shutdownCancel()

		if err := server.Shutdown(shutdownCtx); err != nil {
			log.Printf("Web server shutdown error: %v", err)
		}

		// 等待API服务器关闭
		<-apiServerDone

		return nil
	},
}

// 处理终止信号
func handleSignals(cancel context.CancelFunc) {
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan
	log.Println("Received termination signal, shutting down servers...")
	cancel()
}

func init() {
	WappCmd.Flags().StringVar(&host, "host", "127.0.0.1", "Web server host address")
	WappCmd.Flags().IntVar(&port, "port", 8080, "Web server port")
	WappCmd.Flags().StringVar(&s_host, "s_host", "127.0.0.1", "API server host address")
	WappCmd.Flags().IntVar(&s_port, "s_port", 8280, "API server port")
	WappCmd.Flags().StringVar(&webDir, "web-dir", "web", "Web directory path")
}
