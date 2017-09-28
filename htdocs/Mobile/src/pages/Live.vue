
<template>
  <div>
    <!-- 后退和静音组件 -->
    <top-nav @on-click-top-back="onClickTopBack" @on-click-top-mute="onClickTopMute"></top-nav>
    <!-- 视频播放组件 -->
    <scroller lock-x lock-y :scrollbar-x=false ref="videoScroll" height="videoHeight">
      <video-player class="vjs-custom-skin"
                    ref="videoPlayer"
                    :options="playerOptions"
                    :playsinline="true"
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
import { mapState } from 'vuex'

export default {
  components: {
    XButton,
    Scroller,
    ListView,
    TopNav
  },
  props: {
    videoHeight: {
      type: String,
      default: '210px'
    }
  },
  data () {
    return {
      arrGallery: [],
      curGalPage: 1,
      isLive: true,
      isDispEnd: false,
      boxGround: 'default-90.png',
      liveParams: this.$route.params,
      videoParams: this.$route.params,
      playerOptions: {
        autoplay: true,
        muted: true,
        language: 'zh-CN',
        height: this.videoHeight,
        sources: [{
          withCredentials: false,
          type: 'application/x-mpegURL',
          src: 'http://192.168.1.70:8080/live/live4.m3u8'
        }],
        controlBar: {
          timeDivider: false,
          durationDisplay: false,
          remainingTimeDisplay: false
        },
        html5: {hls: {withCredentials: false}},
        poster: '../../static/live-on.png'
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
    ...mapState({
      fastClick: state => state.vux.fastClick
    }),
    iconClass: function () {
      switch (this.videoParams.stream_prop) {
        case '0': return 'fa-camera'
        case '1': return 'fa-file-video-o'
        case '2': return 'fa-arrow-circle-o-down'
        default: return 'fa-file-video-o'
      }
    }
  },
  deactivated () {
    this.$destroy()
  },
  mounted () {
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
    this.fastClick.attach(this.$refs.galScroller.$el)
    // 向服务器发起点击累加命令，由服务器累加计数，使用服务器反馈的结果更新计数器...
    this.doSaveClick(this.videoParams)
  },
  methods: {
    onClickLive () {
      // 如果已经是直播状态，直接返回...
      if (this.isLive) { return }
      // 设置直播标志，保存参数...
      this.isLive = true
      // 保存当前数据对象...
      this.videoParams = this.liveParams
      // 直接改变播放连接地址和海报地址...
      this.playerOptions.sources[0].type = 'application/x-mpegURL'
      this.playerOptions.sources[0].src = 'http://192.168.1.70:8080/live/live4.m3u8'
      this.playerOptions.poster = '../../static/live-on.png'
      // 隐藏时间轴显示信息...
      this.playerOptions.controlBar.timeDivider = false
      this.playerOptions.controlBar.durationDisplay = false
      // 向服务器发起点击累加命令，由服务器累加计数，使用服务器反馈的结果更新本地界面的计数器...
      this.doSaveClick(this.videoParams)
    },
    onClickVod (item) {
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
      let theUrl = 'http://192.168.1.70/wxapi.php/MobileMonitor/saveClick/type'
      theUrl += this.isLive ? ('/live/camera_id/' + item.camera_id) : ('/vod/record_id/' + item.record_id)
      that.$root.$http.get(theUrl)
        .then((response) => {
          console.log('vod: record_id => %s, s_click => %d, c_click => %s)', (this.isLive ? item.camera_id : item.record_id), response.data, item.clicks)
          item.clicks = response.data
        })
        .catch((error) => {
          console.log(error)
        })
    },
    loadMore (theScroller) {
      this.loadGallery(this.videoParams.camera_id, theScroller)
    },
    loadGallery (theCameraID, theScroller) {
      // 保存当前对象...
      let that = this
      // 获取对应的科目数据...
      that.$root.$http.get('http://192.168.1.70/wxapi.php/MobileMonitor/getRecord/p/' + that.curGalPage + '/camera_id/' + theCameraID)
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