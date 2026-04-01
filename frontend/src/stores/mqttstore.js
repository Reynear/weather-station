import { computed, ref } from 'vue'
import { defineStore } from 'pinia'
import mqtt from 'mqtt'
import { defaultMetricUnits } from '../lib/analysisMetrics'

const mqttHost = 'www.yanacreations.com'
const mqttTopic = '620171008_sub'
const mqttWebsocketUrl =
  import.meta.env.VITE_MQTT_WS_URL?.trim() || `ws://${mqttHost}:9001/mqtt`
const maxHistoryPoints = 60

const liveMetrics = [
  {
    key: 'temperature',
    label: 'Temperature',
    unit: '°C',
    icon: 'i-lucide-thermometer',
    iconClass: 'text-rose-400',
  },
  {
    key: 'humidity',
    label: 'Humidity',
    unit: '%',
    icon: 'i-lucide-droplets',
    iconClass: 'text-sky-400',
  },
  {
    key: 'pressure',
    label: 'Pressure',
    unit: 'hPa',
    icon: 'i-lucide-gauge',
    iconClass: 'text-violet-400',
  },
  {
    key: 'soil_moisture',
    label: 'Soil',
    unit: '%',
    icon: 'i-lucide-sprout',
    iconClass: 'text-emerald-400',
  },
  {
    key: 'altitude',
    label: 'Altitude',
    unit: 'm',
    icon: 'i-lucide-mountain',
    iconClass: 'text-amber-300',
  },
  {
    key: 'heat_index',
    label: 'Heat Index',
    unit: '°C',
    icon: 'i-lucide-flame',
    iconClass: 'text-orange-400',
  },
]

const buildMetricState = (value) => Object.fromEntries(
  liveMetrics.map(({ key }) => [key, typeof value === 'function' ? value() : value])
)

const toNumber = (value) => {
  if (typeof value === 'number') return Number.isFinite(value) ? value : null
  if (typeof value === 'string' && value.trim() !== '') {
    const parsed = Number(value)
    return Number.isFinite(parsed) ? parsed : null
  }
  return null
}

const appendPoint = (series, value, timestamp) => {
  const numeric = toNumber(value)
  if (numeric == null) return series

  return [...series, { x: timestamp, y: numeric }].slice(-maxHistoryPoints)
}

let client = null
let subscriberCount = 0

export const useMqttStore = defineStore('mqtt', () => {
  const rawReadings = ref(buildMetricState(null))
  const history = ref(buildMetricState(() => []))
  const connectionState = ref('connecting')
  const hasReceivedMessage = ref(false)
  const metricUnits = ref({ ...defaultMetricUnits })

  const applyPayload = (payload) => {
    const timestamp = Date.now()

    history.value = Object.fromEntries(
      liveMetrics.map(({ key }) => [key, appendPoint(history.value[key], payload[key], timestamp)])
    )

    rawReadings.value = Object.fromEntries(
      liveMetrics.map(({ key }) => [key, toNumber(payload[key])])
    )
  }

  const connectClient = () => {
    if (client) return

    client = mqtt.connect(mqttWebsocketUrl, {
      clientId: `620171008-web-${Math.random().toString(16).slice(2, 8)}`,
      connectTimeout: 5000,
      reconnectPeriod: 3000,
      clean: true,
      protocolVersion: 4,
    })

    client.on('connect', () => {
      connectionState.value = 'connected'
      client.subscribe(mqttTopic, (error) => {
        if (error) connectionState.value = 'error'
      })
    })

    client.on('reconnect', () => {
      connectionState.value = 'connecting'
    })

    client.on('offline', () => {
      connectionState.value = 'connecting'
    })

    client.on('close', () => {
      if (connectionState.value !== 'error') {
        connectionState.value = 'connecting'
      }
    })

    client.on('error', () => {
      connectionState.value = 'error'
    })

    client.on('message', (topic, message) => {
      if (topic !== mqttTopic) return

      try {
        hasReceivedMessage.value = true
        applyPayload(JSON.parse(message.toString()))
      } catch {
        // Ignore malformed payloads and keep the last valid reading visible.
      }
    })
  }

  const disconnectClient = () => {
    if (!client) return

    client.end(true)
    client = null
  }

  const startLiveStream = () => {
    subscriberCount += 1
    connectClient()
  }

  const stopLiveStream = () => {
    subscriberCount -= 1
    if (subscriberCount <= 0) {
      subscriberCount = 0
      disconnectClient()
    }
  }

  const setMetricUnit = (metric, unit) => {
    if (!(metric in metricUnits.value)) return

    metricUnits.value = {
      ...metricUnits.value,
      [metric]: unit,
    }
  }

  const statusBadge = computed(() => {
    if (connectionState.value === 'connected') {
      return { color: 'success', label: 'Connected' }
    }

    if (connectionState.value === 'error') {
      return { color: 'error', label: 'Error' }
    }

    return { color: 'neutral', label: 'Connecting' }
  })

  const statusAlert = computed(() => {
    if (connectionState.value === 'error') {
      return {
        color: 'error',
        title: 'Connection issue',
        description: 'Connection failed.',
      }
    }

    if (connectionState.value === 'connected' && !hasReceivedMessage.value) {
      return {
        color: 'warning',
        title: 'Waiting for data',
        description: 'No readings yet.',
      }
    }

    return null
  })

  const displayMetrics = computed(() => {
    return liveMetrics.map((metric) => ({
      ...metric,
      value: rawReadings.value[metric.key],
    }))
  })

  return {
    rawReadings,
    history,
    connectionState,
    hasReceivedMessage,
    metricUnits,
    statusBadge,
    statusAlert,
    displayMetrics,
    startLiveStream,
    stopLiveStream,
    setMetricUnit,
  }
}, {
  persist: {
    pick: ['metricUnits'],
  },
})
