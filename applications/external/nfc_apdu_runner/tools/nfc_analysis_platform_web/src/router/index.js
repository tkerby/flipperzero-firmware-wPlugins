import { createRouter, createWebHistory } from 'vue-router';
import Home from '../views/Home.vue';
import TlvView from '../views/TlvView.vue';
import NardView from '../views/NardView.vue';
import SystemView from '../views/SystemView.vue';

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

export default router; 