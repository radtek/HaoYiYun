<template>
  <div>
    <div class="weui_msg msg_top" :style="{display: isError ? 'block' : 'none'}">
      <div class="weui_icon_area" style="margin-bottom: 15px;">
        <i class="weui_icon_safe weui_icon_safe_warn"></i>
      </div>
      <div class="weui_text_area">
        <h2 class="weui_msg_title" style="font-size: 16px">{{msg_title}}</h2>
        <p class="weui_msg_desc">{{msg_desc}}</p>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  props: {
    isError: {
      type: Boolean,
      default: false
    },
    msg_title: {
      type: String,
      default: ''
    },
    msg_desc: {
      type: String,
      default: ''
    }
  },
  mounted () {
  }
}
</script>

<style lang="less">
@import "~vuxx/src/styles/weui/icon/weui_icon_font";
@import '~vuxx/src/styles/weui/widget/weui_page/weui_msg';
.msg_top {
  padding-top: 15px;
}
</style>
