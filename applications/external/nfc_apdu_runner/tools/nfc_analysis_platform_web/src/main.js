import { createApp } from 'vue';
import { createRouter, createWebHistory } from 'vue-router';
import { createPinia } from 'pinia';
import App from './App.vue';
import './assets/main.css';
import i18n from './i18n';

// 导入路由组件
import Home from './views/Home.vue';
import TlvView from './views/TlvView.vue';
import NardView from './views/NardView.vue';
import SystemView from './views/SystemView.vue';

// 创建路由实例
const router = createRouter({
    history: createWebHistory(import.meta.env.BASE_URL),
    routes: [
        {
            path: '/',
            name: 'home',
            component: Home
        },
        {
            path: '/tlv',
            name: 'tlv',
            component: TlvView
        },
        {
            path: '/nard',
            name: 'nard',
            component: NardView
        },
        {
            path: '/system',
            name: 'system',
            component: SystemView
        }
    ]
});

const app = createApp(App);

app.use(createPinia());
app.use(router);
app.use(i18n);

app.mount('#app'); 