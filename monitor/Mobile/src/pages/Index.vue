
<template>
</template>

<script>
export default {
  deactivated () {
    this.$destroy()
  },
  mounted () {
    this.$router.push('Home')
    // this.$store.commit('updateLoadingStatus', {isLoading: false})
  }
}
</script>