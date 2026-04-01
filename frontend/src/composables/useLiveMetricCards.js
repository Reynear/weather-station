import { computed } from 'vue'
import { storeToRefs } from 'pinia'
import { useMqttStore } from '../stores/mqttstore'
import {
  convertMetricValue,
  formatMetricValue,
  resolveMetricUnit,
  resolveMetricUnitKey,
  resolveMetricUnitOptions,
} from '../lib/metricFormatting'

export const useLiveMetricCards = () => {
  const mqttStore = useMqttStore()
  const { displayMetrics, metricUnits, statusAlert, statusBadge } = storeToRefs(mqttStore)

  const liveMetrics = computed(() => {
    return displayMetrics.value.map((metric) => {
      const unitKey = resolveMetricUnitKey(metric.key, metricUnits.value)
      const unit = resolveMetricUnit(metric.key, metricUnits.value)
      const unitOptions = resolveMetricUnitOptions(metric.key)
      const convertedValue = convertMetricValue(metric.key, metricUnits.value, metric.value)

      return {
        ...metric,
        unit,
        unitKey,
        unitOptions,
        value: formatMetricValue(convertedValue, unit, '--'),
      }
    })
  })

  const setUnit = (metric, unit) => {
    mqttStore.setMetricUnit(metric, unit)
  }

  return {
    liveMetrics,
    statusAlert,
    statusBadge,
    setUnit,
  }
}
