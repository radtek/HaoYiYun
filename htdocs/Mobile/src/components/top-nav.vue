<template>
  <div>
    <!-- 左侧返回按钮 -->
    <div class="left-back" @click="onClickBack"></div>
    <div class="left-arrow" @click="onClickBack"></div>
    <!-- 右侧静音按钮 -->
    <div class="right-mute"></div>
    <div class="right-vol" @click="onClickMute">
      <i class="vjs-icon-volume-high"></i>
    </div>
  </div>
</template>

<script>
export default {
  methods: {
    onClickBack () {
      this.$emit('on-click-top-back')
    },
    onClickMute () {
      this.$emit('on-click-top-mute')
    }
  },
  mounted () {
  }
}
</script>

<style lang="less">
.left-back {
  z-index: 100;
  opacity: 0.5;
  background: #666;
  border-radius: 20px;
  position: absolute;
  width: 30px;
  height: 30px;
  top: 15px;
  left: 15px;
}
.left-arrow {
  position: absolute;
  z-index: 110;
  left: 15px;
  top: 15px;
  &:before {
    content: "";
    width: 12px;
    height: 12px;
    position: absolute;
    border: 1px solid #fff;
    border-width: 2px 0 0 2px;
    transform: rotate(315deg);
    left: 11px;
    top: 8px;
  }
}
.right-mute {
  z-index: 100;
  opacity: 0.5;
  background: #666;
  border-radius: 20px;
  position: absolute;
  width: 30px;
  height: 30px;
  top: 15px;
  right: 15px;
}
.right-vol {
  position: absolute;
  z-index: 110;
  top: 11px;
  right: 13px;
  width: 30px;
  color: #fff;
  font-size: 25px;
}
</style>
