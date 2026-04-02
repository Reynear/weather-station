<template>
  <div class="flex flex-col gap-6">
    <UCard>
      <template #header>
        <div>
          <h2 class="text-2xl font-semibold">Summary</h2>
          <p>Select dates to load summary stats and charts.</p>
        </div>
      </template>

      <div class="grid gap-4 lg:grid-cols-[1fr_1fr_auto] lg:items-end">
        <div class="flex flex-col gap-2">
          <label class="text-sm font-medium">Start Date</label>
          <UPopover :content="{ align: 'start' }">
            <UButton
              color="neutral"
              variant="outline"
              icon="i-lucide-calendar"
              class="w-full justify-between font-normal"
            >
              {{ startDateLabel }}
            </UButton>

            <template #content="{ close }">
              <UCalendar
                v-model="startDate"
                class="p-2"
                @update:model-value="close()"
              />
            </template>
          </UPopover>
        </div>

        <div class="flex flex-col gap-2">
          <label class="text-sm font-medium">End Date</label>
          <UPopover :content="{ align: 'start' }">
            <UButton
              color="neutral"
              variant="outline"
              icon="i-lucide-calendar"
              class="w-full justify-between font-normal"
            >
              {{ endDateLabel }}
            </UButton>

            <template #content="{ close }">
              <UCalendar
                v-model="endDate"
                class="p-2"
                @update:model-value="close()"
              />
            </template>
          </UPopover>
        </div>

        <UButton :loading="loading" class="justify-center" @click="loadSummary">
          {{ loading ? 'Loading...' : 'Load' }}
        </UButton>
      </div>
    </UCard>

    <UAlert
      v-if="error"
      color="error"
      title="Summary unavailable"
      :description="error"
    />

    <UAlert
      v-else-if="!fetched"
      title="Choose a date range"
      description="Select dates, then load the summary."
    />

    <UCard v-else>
      <template #header>
        <div class="flex flex-col gap-4 lg:flex-row lg:items-start lg:justify-between">
          <div>
            <h3 class="text-lg font-semibold">Overview</h3>
            <p>Compare min, max, avg, latest, and the raw trend.</p>
          </div>

          <div class="flex flex-wrap items-center gap-2 text-sm text-slate-400">
            <UBadge color="neutral" variant="subtle">{{ selectedRangeLabel }}</UBadge>
            <UBadge color="neutral" variant="outline">{{ totalReadingCountLabel }}</UBadge>
          </div>
        </div>
      </template>

      <UTabs
        v-model="activeMetricTab"
        :items="metricTabs"
        :unmount-on-hide="false"
        class="w-full"
      >
        <template #content>
          <div class="flex flex-col gap-6">
            <div class="flex flex-col gap-4 lg:flex-row lg:items-start lg:justify-between">
              <div class="flex flex-col gap-2">
                <div class="flex items-center gap-3">
                  <h4 class="text-base font-semibold">{{ activeMetric.label }}</h4>
                  <UBadge color="neutral" variant="subtle">
                    {{ activeAggregate.count }} readings
                  </UBadge>
                </div>

                <p class="text-sm text-slate-300">{{ activeMetric.description }}</p>
                <p class="text-sm text-slate-400">{{ activeSummary }}</p>
              </div>

              <div class="flex flex-wrap items-center gap-3 self-start">
                <UBadge color="neutral" variant="outline">{{ activeUnit.label }}</UBadge>

                <div
                  v-if="activeUnitOptions.length > 1"
                  class="inline-flex rounded-xl border border-slate-800 bg-slate-950/60 p-1"
                >
                  <UButton
                    v-for="option in activeUnitOptions"
                    :key="option.value"
                    :color="activeUnitKey === option.value ? 'primary' : 'neutral'"
                    :variant="activeUnitKey === option.value ? 'solid' : 'ghost'"
                    size="sm"
                    @click="setActiveUnit(option.value)"
                  >
                    {{ option.toggleLabel }}
                  </UButton>
                </div>
              </div>
            </div>

            <div class="grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
              <UCard v-for="stat in statCards" :key="stat.label">
                <template #header>
                  <span class="text-sm font-medium text-slate-400">{{ stat.label }}</span>
                </template>

                <div class="flex flex-col gap-2">
                  <div class="text-2xl font-semibold">{{ stat.value }}</div>
                  <p class="text-sm text-slate-400">{{ stat.meta }}</p>
                </div>
              </UCard>
            </div>

            <UAlert
              v-if="!hasMetricData"
              color="warning"
              title="No data"
              description="No valid readings in this range."
            />

            <apexchart
              type="line"
              height="320"
              :options="activeChartOptions"
              :series="activeChartSeries"
            />
          </div>
        </template>
      </UTabs>
    </UCard>
  </div>
</template>

<script setup>
import { useAnalysisView } from '../composables/useAnalysisView'

const {
  metricTabs,
  startDate,
  endDate,
  startDateLabel,
  endDateLabel,
  loading,
  error,
  fetched,
  selectedRangeLabel,
  totalReadingCountLabel,
  activeMetricTab,
  activeMetric,
  activeUnitOptions,
  activeUnitKey,
  activeUnit,
  activeAggregate,
  hasMetricData,
  statCards,
  activeSummary,
  activeChartSeries,
  activeChartOptions,
  setActiveUnit,
  loadSummary,
} = useAnalysisView()
</script>
