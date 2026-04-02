<script setup>
const navigationItems = [
  { label: 'Live', to: '/live' },
  { label: 'Live Graphs', to: '/live-graphs' },
  { label: 'Analysis', to: '/analysis' },
]
</script>

<template>
  <UApp>
    <UContainer class="py-6">
      <div class="flex flex-col gap-6">
        <div class="flex flex-col gap-3">
          <div>
            <p class="text-sm">Weather Station</p>
            <h1 class="text-2xl font-semibold">Dashboard</h1>
          </div>

          <UNavigationMenu :items="navigationItems" />
        </div>

        <RouterView />
      </div>
    </UContainer>
  </UApp>
</template>
