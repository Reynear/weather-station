import { onMounted, onUnmounted } from 'vue'
import { useMqttStore } from '../stores/mqttstore'

export const useLiveStream = () => {
  const mqttStore = useMqttStore()

  onMounted(() => {
    mqttStore.startLiveStream()
  })

  onUnmounted(() => {
    mqttStore.stopLiveStream()
  })
}
