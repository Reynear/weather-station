import { ref } from 'vue'
import { defineStore } from 'pinia'
import { buildApiUrl } from '../lib/api'

export const useAppStore = defineStore('app', () => {
  const analysisRows = ref([])
  const analysisAggregates = ref({})
  const analysisRange = ref({ startIso: '', endIso: '' })
  const analysisLoading = ref(false)
  const analysisError = ref('')
  const analysisFetched = ref(false)

  const todaySummary = ref(null)
  const todaySummaryLoading = ref(false)
  const todaySummaryError = ref('')

  const resetAnalysisState = () => {
    analysisRows.value = []
    analysisAggregates.value = {}
    analysisRange.value = { startIso: '', endIso: '' }
    analysisFetched.value = false
  }

  const apiGet = async (path) => {
    try {
      const response = await fetch(buildApiUrl(path), { method: 'GET' })
      const data = await response.json().catch(() => null)

      if (!response.ok) {
        throw new Error(data?.error || 'request failed')
      }

      return data
    } catch (error) {
      console.log(`API GET error: ${error}`)
      throw error
    }
  }

  const fetchAnalysisRange = async (startIso, endIso) => {
    const searchParams = new URLSearchParams({
      start_time: startIso,
      end_time: endIso,
    })
    const data = await apiGet(`/api/analysis?${searchParams.toString()}`)
    return Array.isArray(data) ? data : []
  }

  const fetchAnalysisAggregates = async (startIso, endIso) => {
    const searchParams = new URLSearchParams({
      start_time: startIso,
      end_time: endIso,
    })
    const data = await apiGet(`/api/analysis/aggregates?${searchParams.toString()}`)
    return data?.metrics ?? {}
  }

  const loadAnalysisSummary = async (startDate, endDate) => {
    const startDateString = startDate?.toString?.() ?? ''
    const endDateString = endDate?.toString?.() ?? ''

    if (!startDateString || !endDateString) {
      analysisError.value = 'Select both dates.'
      resetAnalysisState()
      return
    }

    if (startDateString > endDateString) {
      analysisError.value = 'Start date must come first.'
      resetAnalysisState()
      return
    }

    analysisLoading.value = true
    analysisError.value = ''
    analysisFetched.value = false

    const startIso = `${startDateString}T00:00:00Z`
    const endIso = `${endDateString}T23:59:59.999Z`

    try {
      const [rows, aggregates] = await Promise.all([
        fetchAnalysisRange(startIso, endIso),
        fetchAnalysisAggregates(startIso, endIso),
      ])

      analysisRows.value = rows
      analysisAggregates.value = aggregates
      analysisRange.value = { startIso, endIso }
      analysisFetched.value = true
    } catch {
      analysisError.value = 'Could not load summary.'
      resetAnalysisState()
    } finally {
      analysisLoading.value = false
    }
  }

  const fetchTodaySummary = async () => {
    todaySummaryLoading.value = true
    todaySummaryError.value = ''

    try {
      todaySummary.value = await apiGet('/api/summary/today')
      return todaySummary.value
    } catch {
      todaySummary.value = null
      todaySummaryError.value = 'Could not load today summary.'
      return null
    } finally {
      todaySummaryLoading.value = false
    }
  }

  return {
    analysisRows,
    analysisAggregates,
    analysisRange,
    analysisLoading,
    analysisError,
    analysisFetched,
    todaySummary,
    todaySummaryLoading,
    todaySummaryError,
    apiGet,
    fetchAnalysisRange,
    fetchAnalysisAggregates,
    loadAnalysisSummary,
    fetchTodaySummary,
  }
}, {
  persist: {
    pick: [
      'analysisRows',
      'analysisAggregates',
      'analysisRange',
      'analysisFetched',
      'todaySummary',
    ],
  },
})
