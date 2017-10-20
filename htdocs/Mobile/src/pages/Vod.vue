
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
    <!-- 相关视频组件 -->
    <scroller lock-x :scrollbar-y=true use-pullup :pullup-config="pullupConfig" @pullup:loading="loadMore" ref="galScroller">
      <div><!-- 必须包含这个div容器，否则scroller无法拖动 -->
        <div class="thumb_name">{{videoParams.subject_name}} {{videoParams.grade_type}} {{videoParams.grade_name}} {{videoParams.camera_name}} {{videoParams.teacher_name}} {{videoParams.title_name}}</div>
        <div class="thumb_date">
          <span><i class="fa fa-clock-o">&nbsp;{{videoParams.created}}</i></span>
          <span style="float: right;"><i class="fa fa-play-circle-o">&nbsp;{{videoParams.clicks}}次</i></span>
        </div>
        <div class="thumb_split"></div>
        <ListView :focusRecord="videoParams.record_id" :boxGround="boxGround" :list="arrGallery" @on-click-list-item="onClickListView" ref="listVod"></ListView>
        <div v-show="isDispEnd" class="endScroll" ref="endScroll">没有更多内容了</div>
      </div>
    </scroller>
  </div>
</template>

<script>
import Scroller from 'vuxx-components/scroller'
import ListView from '@/components/ListView'
import TopNav from '@/components/top-nav'

export default {
  components: {
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
      boxGround: '',
      isDispEnd: false,
      videoParams: this.$route.params,
      playerOptions: {
        autoplay: true,
        muted: true,
        loop: false,
        language: 'zh-CN',
        height: this.videoHeight,
        playbackRates: [1.0, 1.5, 2.0],
        sources: [{
          type: 'video/mp4',
          src: this.$route.params.file_fdfs
        }],
        poster: this.$route.params.image_fdfs
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
    // 更新默认的背景小图片 => 加上访问路径...
    this.boxGround = '/wxapi/public/images/default-90.png'
    // 监听横屏事件...
    window.addEventListener('orientationchange', this.onChangeOrientation, false)
    // 如果没有发现有效的参数内容，直接跳转回去 => destroy 很重要...
    if (typeof (this.$route.params.file_fdfs) === 'undefined') {
      history.back()
      this.$destroy()
      return
    }
    // 无论是否设置自动播放，都需要执行关闭等待框，videojs内部会也有等待框...
    // 提前关闭全局等待框，用户体验交给videojs去处理...
    // 只有设置自动播放，才会触发 Canplay 事件...
    this.$store.commit('updateLoadingStatus', {isLoading: false})
    // 不要用setTimeout去关闭等待框，会造成移动端响应缓慢...
    // 不要采用播放错误事件去关闭等待框，用户体验会不太好...
    /* setTimeout(() => {
      this.$store.commit('updateLoadingStatus', {isLoading: false})
    }, 500) */
    // 加载与当前记录对应的相关记录...
    this.$refs.endScroll.style.height = '250px'
    this.$refs.endScroll.style.marginTop = '5px'
    this.loadGallery(this.videoParams.subject_id, this.$refs.galScroller)
    // 进行局部对象定向绑定 => 绑定相关的列表数据框...
    this.$store.state.vux.fastClick.attach(this.$refs.galScroller.$el)
    // 向服务器发起点击累加命令，由服务器累加计数，使用服务器反馈的结果更新计数器...
    this.doSaveClick(this.videoParams)
  },
  computed: {
    player () {
      return this.$refs.videoPlayer.player
    }
  },
  methods: {
    onClickListView (item) {
      // 保存当前数据对象...
      this.videoParams = item
      // 直接改变播放连接地址和海报地址...
      this.playerOptions.sources[0].src = this.videoParams.file_fdfs
      this.playerOptions.poster = this.videoParams.image_fdfs
      // 向服务器发起点击累加命令，由服务器累加计数，使用服务器反馈的结果更新本地界面的计数器...
      this.doSaveClick(this.videoParams)
    },
    doSaveClick (item) {
      let that = this
      let theUrl = '/wxapi.php/MobileMonitor/saveClick/type/vod/record_id/' + item.record_id
      that.$root.$http.get(theUrl)
        .then((response) => {
          console.log('vod: record_id => %s, s_click => %d, c_click => %s)', item.record_id, response.data, item.clicks)
          item.clicks = response.data
        })
        .catch((error) => {
          console.log(error)
        })
    },
    loadMore (theScroller) {
      this.loadGallery(this.videoParams.subject_id, theScroller)
    },
    loadGallery (theSubjectID, theScroller) {
      // 保存当前对象...
      let that = this
      let theUrl = '/wxapi.php/MobileMonitor/getGallery/p/' + that.curGalPage + '/subject_id/' + theSubjectID
      // 获取对应的科目数据...
      that.$root.$http.get(theUrl)
        .then((response) => {
          // 首先，将获取的有效数据叠加起来，丢掉无效数据...
          if ((response.data instanceof Array) && (response.data.length > 0)) {
            that.arrGallery = that.arrGallery.concat(response.data)
          }
          // 第一次获取的记录条数小于10条，说明数据读取完毕了，或者，新数据为空数据，都说明数据已经读取完毕了...
          if (((that.curGalPage === 1) && (response.data.length < 10)) || !(response.data instanceof Array)) {
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
          this.onClickListView(theItem)
          // 结束，跳出循环...
          return
        }
      }
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
    },
    playerStateChanged (playerCurrentState) {
      // 所有的事件都会经过这里，打印信息量太大...
      // console.log('player current update state', playerCurrentState)
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
.thumb_split {
  height: 5px;
  background-color: rgb(230,230,230);
  border-top: 1px solid rgb(220,220,220);
  border-bottom: 1px solid rgb(220,220,220);
}
</style>