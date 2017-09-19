
<template>
  <div>
    <!--header slot
    <div class="vux-demo-header-box">
      <x-header :left-options="{showBack: true}">点播播放页面</x-header>
    </div>-->
    <!-- 左侧返回按钮 -->
    <div class="left-back" @click="onClickBack"></div>
    <div class="left-arrow" @click="onClickBack"></div>
    <!-- 右侧静音按钮 -->
    <div class="right-mute"></div>
    <div class="right-vol" @click="onClickMute($event)">
      <i class="vjs-icon-volume-high"></i>
    </div>
    <scroller lock-x lock-y :scrollbar-x=false ref="videoScroll">
      <video-player class="vjs-custom-skin"
                     ref="videoPlayer"
                     :options="playerOptions"
                     :playsinline="false"
                     @play="onPlayerPlay($event)"
                     @pause="onPlayerPause($event)"
                     @ended="onPlayerEnded($event)"
                     @loadeddata="onPlayerLoadeddata($event)"
                     @waiting="onPlayerWaiting($event)"
                     @playing="onPlayerPlaying($event)"
                     @timeupdate="onPlayerTimeupdate($event)"
                     @canplay="onPlayerCanplay($event)"
                     @canplaythrough="onPlayerCanplaythrough($event)"
                     @error="onPlayerError($event)"
                     @ready="playerReadyOK($event)"
                     @statechanged="playerStateChanged($event)">
      </video-player>
    </scroller>
  </div>
</template>

<script>
import Scroller from 'vuxx-components/scroller'

export default {
  components: {
    Scroller
  },
  data () {
    return {
      // videojs options
      playerOptions: {
        autoplay: true,
        muted: true,
        loop: false,
        height: '210px',
        language: 'zh-CN',
        playbackRates: [1.0, 1.5, 2.0],
        sources: [{
          type: 'video/mp4',
          src: this.$route.params.file_fdfs
        }],
        poster: this.$route.params.image_fdfs
      }
    }
  },
  created () {
    // console.log('vod - created')
  },
  activated () {
    // console.log('vod - activated')
  },
  deactivated () {
    this.$destroy()
  },
  mounted () {
    // 监听横屏事件...
    window.addEventListener('orientationchange', this.onChangeOrientation, false)
    // 如果没有发现有效的参数内容，直接跳转回去 => destroy 很重要...
    if (typeof (this.$route.params.file_fdfs) === 'undefined') {
      history.back()
      this.$destroy()
      return
    }
    // 默认开启静音，加载成功之后，再关闭静音，就能自动播放...
    this.$refs.videoPlayer.player.muted(false)
    // 无论是否设置自动播放，都需要执行关闭等待框，videojs内部会也有等待框...
    // 提前关闭全局等待框，用户体验交给videojs去处理...
    // 只有设置自动播放，才会触发 Canplay 事件...
    this.$store.commit('updateLoadingStatus', {isLoading: false})
    // 不要用setTimeout去关闭等待框，会造成移动端响应缓慢...
    // 不要采用播放错误事件去关闭等待框，用户体验会不太好...
    /* setTimeout(() => {
      this.$store.commit('updateLoadingStatus', {isLoading: false})
    }, 500) */
  },
  computed: {
    player () {
      return this.$refs.videoPlayer.player
    }
  },
  methods: {
    onChangeOrientation (event) {
      // 有角度变化，进行全屏请求 => 这里不需要还原操作，会引起浏览器位置混乱...
      if (window.orientation !== 0) {
        this.$refs.videoPlayer.player.requestFullscreen()
      }
    },
    onClickBack () {
      // 点击左侧返回箭头...
      history.back()
    },
    onClickMute (event) {
      // 点击右侧静音按钮 => 得到当前对象，当前播放器，当前状态...
      let theVolume = event.currentTarget.firstElementChild
      let thePlayer = this.$refs.videoPlayer.player
      let theMuted = thePlayer.muted()
      // 将静音状态取反，使用videojs内部的按钮设置图标字体状态...
      thePlayer.muted(!theMuted)
      theVolume.setAttribute('class', theMuted ? 'vjs-icon-volume-high' : 'vjs-icon-volume-mute')
    },
    onPlayerPlay (player) {
      console.log('player play!')
    },
    onPlayerPause (player) {
      console.log('player pause!')
    },
    onPlayerEnded (player) {
      console.log('player ended!')
    },
    onPlayerLoadeddata (player) {
      console.log('player Loadeddata!')
    },
    onPlayerWaiting (player) {
      console.log('player Waiting!')
    },
    onPlayerPlaying (player) {
      console.log('player Playing!')
    },
    onPlayerTimeupdate (player) {
      // 这里是更新播放时间状态通知...
      // console.log('player Timeupdate!')
    },
    onPlayerCanplaythrough (player) {
      console.log('player Canplaythrough!')
    },
    onPlayerCanplay (player) {
      console.log('player Canplay!')
      // 可以播放视频了，关闭等待框 => 为了增强体验，mounted 当中执行...
      // this.$store.commit('updateLoadingStatus', {isLoading: false})
    },
    onPlayerError (player) {
      console.log('player Error!')
      // 发生错误，播放失败，关闭等待框 => 为了增强体验，mounted 当中执行...
      // this.$store.commit('updateLoadingStatus', {isLoading: false})
    },
    playerReadyOK (player) {
      // 这是最早的通知，videojs对象加载成功，离播放还有很远距离...
      console.log('player readyok!')
    },
    playerStateChanged (playerCurrentState) {
      // 所有的事件都会经过这里，打印信息量太大...
      // console.log('player current update state', playerCurrentState)
    }
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