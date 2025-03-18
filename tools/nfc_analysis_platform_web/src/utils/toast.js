/*
 * @Author: SpenserCai
 * @Date: 2025-03-16 23:05:13
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-17 18:52:57
 * @Description: file content
 */
import { createApp } from 'vue';
import MessageToast from '@/components/MessageToast.vue';

const toast = {
  show(options) {
    const { message, type = 'default', duration = 3000 } = options;
    
    // 创建一个容器元素
    const container = document.createElement('div');
    document.body.appendChild(container);
    
    // 创建消息组件实例
    const app = createApp(MessageToast, {
      message,
      type,
      duration,
      onClose: () => {
        // 组件关闭后移除容器
        app.unmount();
        document.body.removeChild(container);
      }
    });
    
    // 挂载组件
    app.mount(container);
    
    // 设置自动移除
    setTimeout(() => {
      app.unmount();
      if (document.body.contains(container)) {
        document.body.removeChild(container);
      }
    }, duration + 1000); // 额外添加1秒，确保动画完成
    
    return app;
  },
  
  success(message, duration) {
    return this.show({ message, type: 'success', duration });
  },
  
  error(message, duration) {
    return this.show({ message, type: 'error', duration });
  },
  
  info(message, duration) {
    return this.show({ message, type: 'info', duration });
  }
};

export default toast; 