
<template>
  <div>
    <!-- 后退和静音组件 -->
    <top-nav @on-click-top-back="onClickTopBack" @on-click-top-mute="onClickTopMute" :isError="doCheckError"></top-nav>
    <!-- 视频播放组件 -->
    <scroller lock-x lock-y :scrollbar-x=false ref="videoScroll" :height="videoHeight">
      <error :msg_title="doMsgTitle" :msg_desc="doMsgDesc" :isError="doCheckError"></error>
      <video-player class="vjs-custom-skin" :style="{display: doCheckError ? 'none' : 'block'}"
                    ref="videoPlayer"
                    :options="playerOptions"
                    :playsinline="true"
                    @play="onPlayerPlay($event)"
                    @pause="onPlayerPause($event)"
                    @canplaythrough="onPlayerCanplaythrough($event)"
                    @canplay="onPlayerCanplay($event)"
                    @error="onPlayerError($event)"
                    @ended="onPlayerEnded($event)"
                    @ready="playerReadyOK($event)">
      </video-player>
    </scroller>
    <!-- 相关视频组件 -->
    <scroller lock-x :scrollbar-y=true use-pullup :pullup-config="pullupConfig" @pullup:loading="loadMore" ref="galScroller">
      <div><!-- 必须包含这个div容器，否则scroller无法拖动 -->
        <div class="thumb_name">
          <i class="fa" :class="iconClass" />
          {{videoParams.grade_type}} {{videoParams.grade_name}} {{videoParams.camera_name}} {{videoParams.school_name}}
        </div>
        <div class="thumb_date thumb_ext">
          <span><i class="fa fa-clock-o">&nbsp;{{videoParams.created}}</i></span>
          <span style="float: right;"><i class="fa fa-play-circle-o">&nbsp;{{videoParams.clicks}}次</i></span>
        </div>
        <div class="thumb_live">
          <x-button type="primary" @click="onClickLive">点击切换到直播</x-button>
        </div>
        <div class="thumb_split"></div>
        <ListView :focusRecord="videoParams.record_id" :boxGround="boxGround" :list="arrGallery" @on-click-list-item="onClickVod" ref="listVod"></ListView>
        <div v-show="isDispEnd" class="endScroll" ref="endScroll">没有更多内容了</div>
      </div>
    </scroller>
  </div>
</template>

<script>
import XButton from 'vuxx-components/x-button'
import Scroller from 'vuxx-components/scroller'
import ListView from '@/components/ListView'
import TopNav from '@/components/top-nav'
import Error from '@/components/error'
var qs = require('qs')

export default {
  components: {
    XButton,
    Scroller,
    ListView,
    TopNav,
    Error
  },
  props: {
    videoHeight: {
      type: String,
      default: '210px'
    }
  },
  data () {
    return {
      player_clock: -1,
      arrGallery: [],
      arrHlsAddr: [],
      boxGround: '',
      curGalPage: 1,
      isLive: true,
      isDispEnd: false,
      liveParams: this.$route.params,
      videoParams: this.$route.params,
      playerOptions: {
        autoplay: true,
        muted: true,
        language: 'zh-CN',
        height: this.videoHeight,
        sources: [{
          withCredentials: false,
          type: this.$route.params.arrHlsAddr.hls_type,
          src: this.$route.params.arrHlsAddr.hls_url
        }],
        controlBar: {
          timeDivider: false,
          durationDisplay: false,
          remainingTimeDisplay: false
        },
        html5: {hls: {withCredentials: false}},
        poster: '/wxapi/public/images/live-on.png'
        // 注意：proxyTable解决，不用this.$store.state.vux.ajaxImgPath
        // 注意：这里不能用computed计算值，data() 先于computed()执行...
      },
      pullupConfig: {
        pullUpHeight: 245, // 30 + 210 => 需要对上拉滚动的参数进行偏移修正 => 由于tabBanner的存在...
        height: 245,       // 30 + 210 => 需要对上拉滚动的参数进行偏移修正 => 由于tabBanner的存在...
        content: '上拉加载更多',
        downContent: '松开进行加载',
        upContent: '上拉加载更多',
        loadingContent: '加载中...'
      }
    }
  },
  computed: {
    iconClass: function () {
      switch (this.videoParams.stream_prop) {
        case '0': return 'fa-camera'
        case '1': return 'fa-file-video-o'
        case '2': return 'fa-arrow-circle-o-down'
        default: return 'fa-file-video-o'
      }
    },
    doCheckError: function () {
      // 如果是点播，永远显示静音标志...
      if (!this.isLive) return false
      // 如果是直播，用err_code设置...
      return this.liveParams.arrHlsAddr.err_code
    },
    doMsgTitle: function () {
      // 如果是点播，返回空值...
      if (!this.isLive) return ''
      // 如果是直播，返回err_msg，一定有效...
      return this.liveParams.arrHlsAddr.err_msg
    },
    doMsgDesc: function () {
      // 如果是点播，返回空值...
      if (!this.isLive) return ''
      // 如果是直播，返回err_desc，一定有效...
      return this.liveParams.arrHlsAddr.err_desc
    }
  },
  deactivated () {
    // 通知关闭播放器命令 => 这里一定要发送...
    this.doLivePlayVerify(0)
    // 关闭重置时钟...
    this.resetClock()
    // 销毁当前页面...
    this.$destroy()
  },
  mounted () {
    // 更新默认的背景小图片 => 加上访问路径...
    this.boxGround = '/wxapi/public/images/default-90.png'
    // 监听横屏事件...
    window.addEventListener('orientationchange', this.onChangeOrientation, false)
    // 如果没有发现有效的参数内容，直接跳转回去 => destroy 很重要...
    if (typeof (this.$route.params.camera_id) === 'undefined') {
      history.back()
      this.$destroy()
      return
    }
    // 设置直播标志...
    this.isLive = true
    // 无论是否设置自动播放，都需要执行关闭等待框，videojs内部会也有等待框...
    // 提前关闭全局等待框，用户体验交给videojs去处理...
    // 只有设置自动播放，才会触发 Canplay 事件...
    this.$store.commit('updateLoadingStatus', {isLoading: false})
    // 加载与当前记录对应的相关记录...
    this.$refs.endScroll.style.height = '250px'
    this.$refs.endScroll.style.marginTop = '5px'
    this.loadGallery(this.videoParams.camera_id, this.$refs.galScroller)
    // 进行局部对象定向绑定 => 绑定相关的列表数据框...
    this.$store.state.vux.fastClick.attach(this.$refs.galScroller.$el)
    // 创建汇报时钟，新增点击次数...
    this.buildClock()
  },
  methods: {
    resetClock () {
      clearInterval(this.player_clock)
      this.player_clock = -1
    },
    buildClock () {
      // 获取hls地址有错误不执行汇报命令...
      if (this.liveParams.arrHlsAddr.err_code) { return }
      // 直播地址正确，设置超时汇报事件，每隔12秒调用ajax通知接口...
      let that = this
      that.player_clock = setInterval(function () {
        that.doLivePlayVerify(1)
      }, 12000)
      // 向服务器发起点击累加命令，由服务器累加计数，使用服务器反馈的结果更新计数器...
      that.doSaveClick(that.videoParams)
      console.log('=== buildClock ok ===')
    },
    doGetLiveAddr (inCameraID) {
      // 组合需要远程访问的地址...
      let that = this
      let theUrl = '/wxapi.php/Mobile/getHlsAddr/camera_id/' + inCameraID
      // 设置等待状态，发起异步命令...
      console.log('=== get hls address start ===')
      that.$store.commit('updateLoadingStatus', {isLoading: true})
      that.$root.$http.get(theUrl)
        .then((response) => {
          // 判断返回的hls地址是否有效...
          that.liveParams.arrHlsAddr = response.data
          console.log(response.data.hls_url)
          // 保存当前数据对象...
          that.videoParams = that.liveParams
          // 直接改变播放连接地址和海报地址...
          that.playerOptions.sources[0].type = that.liveParams.arrHlsAddr.hls_type
          that.playerOptions.sources[0].src = that.liveParams.arrHlsAddr.hls_url
          that.playerOptions.poster = '/wxapi/public/images/live-on.png'
          // 隐藏时间轴显示信息...
          that.playerOptions.controlBar.timeDivider = false
          that.playerOptions.controlBar.durationDisplay = false
          // 创建汇报时钟，新增点击次数...
          this.buildClock()
          // 打印返回信息，关闭等待框...
          that.$store.commit('updateLoadingStatus', {isLoading: false})
          console.log('=== get hls address end ===')
        })
        .catch((error) => {
          that.$store.commit('updateLoadingStatus', {isLoading: false})
          console.log(error)
        })
    },
    onClickLive () {
      // 如果已经是直播状态，直接返回...
      if (this.isLive) { return }
      // 设置直播标志，保存参数...
      this.isLive = true
      // 复位时钟，让中转器超时删除已挂载hls直播播放器...
      this.resetClock()
      // 重新异步获取hls地址...
      this.doGetLiveAddr(this.liveParams.camera_id)
    },
    onClickVod (item) {
      /************************************************************************************
      // 重置播放器时钟，不发送通知命令，让中转服务器通过超时检测删除直播播放器...
      // 修正了中转服务器对超时的处理，每隔固定10秒检测一次，以前只能通过事件检测有问题...
      *************************************************************************************/
      this.resetClock()
      // 设置直播标志...
      this.isLive = false
      // 保存当前数据对象...
      this.videoParams = item
      // 直接改变播放连接地址和海报地址...
      this.playerOptions.sources[0].type = 'video/mp4'
      this.playerOptions.sources[0].src = this.videoParams.file_fdfs
      this.playerOptions.poster = this.videoParams.image_fdfs
      // 开启时间轴显示信息...
      this.playerOptions.controlBar.timeDivider = true
      this.playerOptions.controlBar.durationDisplay = true
      // 向服务器发起点击累加命令，由服务器累加计数，使用服务器反馈的结果更新本地界面的计数器...
      this.doSaveClick(this.videoParams)
    },
    doSaveClick (item) {
      let that = this
      let theUrl = '/wxapi.php/Mobile/saveClick/type'
      theUrl += this.isLive ? ('/live/camera_id/' + item.camera_id) : ('/vod/record_id/' + item.record_id)
      that.$root.$http.get(theUrl)
        .then((response) => {
          if (this.isLive) {
            console.log('live: camera_id => %s, s_click => %d, c_click => %s)', item.camera_id, response.data, item.clicks)
          } else {
            console.log('vod: record_id => %s, s_click => %d, c_click => %s)', item.record_id, response.data, item.clicks)
          }
          item.clicks = response.data
        })
        .catch((error) => {
          console.log(error)
        })
    },
    // 直播状态通知接口 => inPlayActive不能用true或false，必须用1或0...
    doLivePlayVerify (inPlayActive) {
      // 获取hls地址有错误时不执行汇报命令...
      if (this.liveParams.arrHlsAddr.err_code) { return }
      // 通过ajax发送异步消息命令给转发服务器...
      let that = this
      let theUrl = '/wxapi.php/RTMP/verify'
      let theCamera = this.liveParams.arrHlsAddr.player_camera
      let thePlayID = this.liveParams.arrHlsAddr.player_id
      // axios的post数据，必须经过qs.stringify处理，否则在php端无法解析，同时，必须加上Content-Type...
      const theData = {rtmp_live: theCamera, player_id: thePlayID, player_type: 1, player_active: inPlayActive}
      that.$root.$http.post(theUrl, qs.stringify(theData), {headers: {'Content-Type': 'application/x-www-form-urlencoded'}})
        .then((response) => {
          // 打印错误信息，关闭定时器...
          if (response.data.err_code > 0) {
            console.log('verify error => ' + response.data.err_msg)
            clearInterval(that.player_clock)
            return
          }
          // 打印成功信息...
          console.log('verify success => player_camera: %s, player_id: %d, player_type: %d, player_active: %d', theCamera, thePlayID, 1, inPlayActive)
        })
        .catch((error) => {
          console.log(error)
          clearInterval(that.player_clock)
        })
    },
    loadMore (theScroller) {
      this.loadGallery(this.videoParams.camera_id, theScroller)
    },
    loadGallery (theCameraID, theScroller) {
      // 保存当前对象...
      let that = this
      let theUrl = '/wxapi.php/Mobile/getRecord/p/' + that.curGalPage + '/camera_id/' + theCameraID
      // 获取对应的科目数据...
      that.$root.$http.get(theUrl)
        .then((response) => {
          // 首先，将获取的有效数据叠加起来，丢掉无效数据...
          if ((response.data instanceof Array) && (response.data.length > 0)) {
            that.arrGallery = that.arrGallery.concat(response.data)
          }
          // 新数据为空数据、不是数组、第一次获取的记录条数小于10条，都说明数据已经读取完毕了...
          if (!(response.data instanceof Array) || ((that.curGalPage === 1) && (response.data.length < 10))) {
            // 设置数据读取完毕 => 必须进行reset+disable，不要使用done...
            that.$nextTick(() => {
              theScroller.$emit('pullup:reset', theScroller.uuid)
              theScroller.$emit('pullup:disable', theScroller.uuid)
              that.isDispEnd = true
            })
            // 直接中断返回...
            return
          }
          // 增加页码，不作为判断依据...
          ++that.curGalPage
          // 重置上拉条 => 使上拉条复位...
          that.$nextTick(() => {
            theScroller.$emit('pullup:reset', theScroller.uuid)
            theScroller.$emit('pullup:enable', theScroller.uuid)
          })
        })
        .catch((error) => {
          console.log(error)
        })
    },
    isAndroid () {
      let uAgent = navigator.userAgent
      return (uAgent.indexOf('Android') > -1 || uAgent.indexOf('Linux') > -1)
    },
    isWeXin () {
      let uAgent = navigator.userAgent.toLowerCase()
      return (uAgent.indexOf('micromessenger') > -1)
    },
    onChangeOrientation (event) {
      // 有角度变化，进行全屏请求 => 这里不需要还原操作，会引起浏览器位置混乱...
      if (window.orientation !== 0) {
        this.$refs.videoPlayer.player.requestFullscreen()
      }
    },
    onClickTopBack () {
      // 点击左侧返回箭头...
      history.back()
    },
    onClickTopMute () {
      // 点击右侧静音按钮 => 得到当前对象，当前播放器，当前状态...
      let theVolume = event.currentTarget.firstElementChild
      let thePlayer = this.$refs.videoPlayer.player
      let theMuted = thePlayer.muted()
      // 将静音状态取反，使用videojs内部的按钮设置图标字体状态...
      thePlayer.muted(!theMuted)
      theVolume.setAttribute('class', theMuted ? 'vjs-icon-volume-high' : 'vjs-icon-volume-mute')
    },
    onPlayerCanplaythrough (player) {
      console.log('player Canplaythrough!')
    },
    onPlayerCanplay (player) {
      console.log('player Canplay!')
    },
    onPlayerError (player) {
      console.log('player Error!')
    },
    onPlayerPlay (player) {
      console.log('player play!')
    },
    onPlayerPause (player) {
      console.log('player pause!')
    },
    onPlayerEnded (player) {
      // 播放正常结束，自动播放下一条...
      console.log('player ended!')
      // 如果记录数无效，直接返回...
      if (this.arrGallery.length <= 0) { return }
      // 利用当前播放焦点记录编号在现有数据列表中查找索引...
      let theFocusID = this.$refs.listVod.focusRecord
      for (var i = 0; i < this.arrGallery.length; ++i) {
        if (theFocusID === this.arrGallery[i].record_id) {
          // 累加焦点索引编号...
          let theNewIndex = i + 1
          // 索引编号越界...
          if (theNewIndex >= this.arrGallery.length) {
            // 将滚动容器返回到顶部，重置索引编号...
            this.$refs.galScroller._xscroll.scrollTop(0, 1000, 'ease-in-out')
            theNewIndex = 0
          }
          // 模拟点击对应的索引编号...
          let theItem = this.arrGallery[theNewIndex]
          this.onClickVod(theItem)
          // 结束，跳出循环...
          return
        }
      }
    },
    playerReadyOK (player) {
      // 这是最早的通知，videojs对象加载成功，离播放还有很远距离...
      console.log('player readyok!')
      // 放在这里处理自动播放，是因为地址变化时，这里会被重新加载一次...
      // 设置微信安卓版解决video标签最上层的问题，需要在mounted之前加载，player.vue 当中修改...
      // this.$el.children[0].setAttribute('x5-video-player-type', 'h5')
      // 如果是微信安卓版，不要自动播放，关闭静音...
      if (this.isAndroid() && this.isWeXin()) {
        this.playerOptions.autoplay = false
        this.playerOptions.muted = false
      } else {
        // 默认开启静音，加载成功之后，再关闭静音，就能自动播放...
        // 备注：如果是第二次更新，iOS就不一定起作用了，即使延时设置，也不能自动播放，安卓浏览器可以自动播放...
        this.$refs.videoPlayer.player.muted(false)
      }
    }
  }
}
</script>
<style lang="less">
.thumb_name {
  margin: 10px 10px 8px;
  font-weight: bold;
  line-height: 18px;
  overflow:hidden; 
  text-overflow:ellipsis;
  display:-webkit-box; 
  -webkit-box-orient:vertical;
  -webkit-line-clamp:2;
}
.thumb_date {
  color: #888;
  height: 16px;
  line-height: 16px;
  overflow: hidden;
  font-size: 14px;
  margin: 8px 10px;
}
.thumb_ext {
  padding: 0px 2px 8px 2px;
  border-bottom: 1px solid #ccc;
}
.thumb_live {
  width: 60%;
  margin: 10px auto;
}
.thumb_split {
  height: 5px;
  background-color: rgb(230,230,230);
  border-top: 1px solid rgb(220,220,220);
  border-bottom: 1px solid rgb(220,220,220);
}

</style>