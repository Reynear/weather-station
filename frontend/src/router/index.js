import { createRouter, createWebHistory } from 'vue-router'
import LiveReadings from '../views/LiveReadings.vue'
import LiveGraphs from '../views/LiveGraphs.vue'
import Analysis from '../views/Analysis.vue'

const routes = [
  { path: '/', redirect: '/live' },
  { path: '/live', component: LiveReadings },
  { path: '/live-graphs', component: LiveGraphs },
  { path: '/analysis', component: Analysis }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
