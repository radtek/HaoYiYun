<template>
  <div>
    <div>
      <loading :show="isLoading" position="absolute" text="加载中"></loading>
    </div>
    <div>
      <transition :name="'vux-pop-' + (direction === 'forward' ? 'in' : 'out')">
        <keep-alive>
          <router-view></router-view>
        </keep-alive>
      </transition>
    </div>
  </div>
</template>

<script>
import Loading from 'vuxx-components/loading'

import { mapState } from 'vuex'

export default {
  components: {
    Loading
  },
  computed: {
    ...mapState({
      direction: state => state.vux.direction,
      isLoading: state => state.vux.isLoading
    })
  },
  mounted () {
    // 会自动路由到根路径下面 => 查看 router/index.js 的实现...
    // this.$router.push('Home')
  }
}
</script>

<style lang="less">
* {
  margin: 0;
  padding: 0;
}
html, body {
  width: 100%;
  /*height: 100%;*/
  overflow-x: hidden;
  -ms-text-size-adjust: 100%;
  -webkit-text-size-adjust: 100%;
}
body {
  background-color: #fbf9fe;
  font-family: "Segoe UI", "Lucida Grande", Helvetica, Arial, "Microsoft YaHei", FreeSans, Arimo, "Droid Sans", "wenquanyi micro hei", "Hiragino Sans GB", "Hiragino Sans GB W3", "FontAwesome", sans-serif;
  font-weight: normal;
  line-height: 1.6;
  color: #333333;
}
.vux-pop-out-enter-active,
.vux-pop-out-leave-active,
.vux-pop-in-enter-active,
.vux-pop-in-leave-active {
  will-change: transform;
  transition: all 500ms;
  height: 100%;
  width: 100%;
  position: absolute;
  backface-visibility: hidden;
  -webkit-backface-visibility:hidden;	 /* Chrome 和 Safari */
  -moz-backface-visibility:hidden; 	   /* Firefox */
  -webkit-perspective: 1000;
          perspective: 1000;
}
.vux-pop-out-enter {
  opacity: 0;
  transform: translate3d(-100%, 0, 0);
}
.vux-pop-out-leave-active {
  opacity: 0;
  transform: translate3d(100%, 0, 0);
}
.vux-pop-in-enter {
  opacity: 0;
  transform: translate3d(100%, 0, 0);
}
.vux-pop-in-leave-active {
  opacity: 0;
  transform: translate3d(-100%, 0, 0);
}
/* height 必须由各个模块自行设定 */
.endScroll {
  color: #999999;
  font-size: 16px;
  text-align: center;
}
</style>
