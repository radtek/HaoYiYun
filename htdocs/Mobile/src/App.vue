<template>
  <div>
    <!--<img v-lazyload="testImg" />
    <input v-focus />-->
    <loading :show="isLoading" position="absolute" text="加载中"></loading>
    <!-- 需要打开 fastclick 否则 scroller 不能点击 -->
    <!-- vux-tab-item 设置了固定的高度 44px -->
    <scroller lock-y :scrollbar-x=false ref="tabScroller">
      <tab :line-width="2" active-color="#fc378c" :animate="true" :style="'width:' + tabWidth + 'px'" @on-index-change="onSubjectChange">
        <tab-item :selected="index === 0" v-for="(item, index) in arrSubject" :key="index">{{item.subject_name}}</tab-item>
      </tab>
    </scroller>
    <scroller lock-x :scrollbar-y=true enable-horizontal-swiping use-pulldown :pulldown-config="pulldownConfig" @pulldown:loading="refresh" use-pullup :pullup-config="pullupConfig" @pullup:loading="loadMore" ref="galScroller">
      <div><!-- 必须包含这个div容器，否则scroller无法拖动 -->
        <swiper v-show="isDispSwiper" :list="arrNewVod" auto loop :threshold=10 height="200px" @on-click-list-item="onClickSwiper"></swiper>
        <galleray v-show="isDispGallery" :isLive="isLive" :boxGround="boxGround" :list="arrGallery" @on-click-list-item="onClickGallery"></galleray>
        <div v-show="isDispEnd" class="endScroll" ref="endScroll">没有更多内容了</div>
      </div>
    </scroller>
  </div>
</template>

<script>
import Tab from 'vuxx-components/tab/tab'
import TabItem from 'vuxx-components/tab/tab-item'
import Swiper from 'vuxx-components/swiper'
import Loading from 'vuxx-components/loading'
import Scroller from 'vuxx-components/scroller'

import Galleray from './components/galleray'

export default {
  components: {
    Tab,
    TabItem,
    Swiper,
    Loading,
    Scroller,
    Galleray
  },
  data () {
    return {
      boxGround: 'default-150.png',
      isDispGallery: true,
      isDispSwiper: true,
      isDispEnd: false,
      isLoading: true,
      isLive: false,
      curSubject: 0,
      curGalPage: 1,
      maxGalPage: 1,
      tabWidth: 300,
      arrSubject: [],
      arrNewVod: [],
      arrGallery: [],
      pulldownConfig: {
        height: 30,
        content: '',
        downContent: '下拉刷新',
        upContent: '释放刷新',
        loadingContent: '加载中...'
      },
      pullupConfig: {
        pullUpHeight: 74, // 30 + 44 => 需要对上拉滚动的参数进行偏移修正 => 由于tabBanner的存在...
        height: 74,       // 30 + 44 => 需要对上拉滚动的参数进行偏移修正 => 由于tabBanner的存在...
        content: '',
        downContent: '松开进行加载',
        upContent: '上拉加载更多',
        loadingContent: '加载中...'
      }
    }
  },
  methods: {
    onSubjectChange (selectIndex) {
      // this.arrSubject[selectIndex].subject_id   => 科目编号...
      // this.arrSubject[selectIndex].subject_name => 科目名称...
      console.log('Subject(%s, %s)', this.arrSubject[selectIndex].subject_id, this.arrSubject[selectIndex].subject_name)
      // 如果当前索引与选中的索引不一样，才更新数据...
      if (this.curSubject !== selectIndex) {
        // 保存当前的科目索引编号 => 并不是数据库编号...
        this.curSubject = selectIndex
        // 获取选定的科目内容...
        this.loadSubject(this.arrSubject[selectIndex].subject_id)
      }
    },
    onClickSwiper (item) {
      if (this.curSubject === 1) {
        console.log('Swiper(Live, %s)', item.id)
      } else {
        console.log('Swiper(Vod, %s)', item.id)
      }
    },
    onClickGallery (item) {
      if (this.curSubject === 1) {
        console.log('Gallery(Live, %s)', item.camera_id)
      } else {
        console.log('Gallery(Vod, %s)', item.record_id)
      }
    },
    refresh (theScroller) {
      let that = this
      let theSubjectID = that.arrSubject[that.curSubject].subject_id
      // 从服务器获取选中科目的最新5条记录数据...
      that.$root.$http.get('http://192.168.1.70/wxapi.php/MobileMonitor/getSwiper/subject_id/' + theSubjectID)
        .then((response) => {
          // 设置 swiper 标签数据 => 最新5个录像文件...
          that.arrNewVod = response.data
          // swiper数据不是数组，不显示swiper对象...
          that.isDispSwiper = true
          if (!(that.arrNewVod instanceof Array)) {
            that.isDispSwiper = false
            that.arrNewVod = []
          }
          // 重置下拉滚动框 => 必须通过nextTick...
          that.$nextTick(() => {
            theScroller.$emit('pulldown:reset', theScroller.uuid)
          })
        })
        .catch((error) => {
          console.log(error)
        })
    },
    loadMore (theScroller) {
      // 保存当前处理对象...
      let that = this
      // 判断页码是否超过了最大值...
      if (that.curGalPage > that.maxGalPage) {
        // 设置数据读取完毕 => 必须进行reset+disable，不要使用done...
        that.$nextTick(() => {
          theScroller.$emit('pullup:disable', theScroller.uuid)
          theScroller.$emit('pullup:reset', theScroller.uuid)
          that.isDispEnd = true
        })
        // 直接跳转返回...
        return
      }
      // 当前页码小于或等于最大页码，继续根据页码请求数据...
      let theSubjectID = that.arrSubject[that.curSubject].subject_id
      that.$root.$http.get('http://192.168.1.70/wxapi.php/MobileMonitor/getGallery/p/' + that.curGalPage + '/subject_id/' + theSubjectID)
        .then((response) => {
          // 叠加记录...
          that.arrGallery = that.arrGallery.concat(response.data)
          // 增加页码 => 一定增加页码，作为判断依据...
          ++that.curGalPage
          // 重置上拉条 => 使上拉条复位...
          that.$nextTick(() => {
            theScroller.$emit('pullup:reset', theScroller.uuid)
          })
        })
        .catch((error) => {
          console.log(error)
        })
    },
    loadSubject (theSubjectID) {
      let that = this
      that.curGalPage = 1
      that.maxGalPage = 1
      that.isLive = false
      that.isLoading = true
      that.isDispEnd = false
      that.isDispSwiper = true
      that.isDispGallery = true
      that.boxGround = 'default-150.png'
      that.$refs.endScroll.style.lineHeight = ''
      // 如果是直播，使用blank.gif...
      if (theSubjectID === -2) {
        that.boxGround = 'blank.gif'
        that.isLive = true
      }
      // 这里必须让下拉滚动容器返回到顶部...
      that.$refs.galScroller._xscroll.scrollTop(0, 1000, 'ease-in-out')
      // 向服务器请求科目数据内容...
      that.$root.$http.get('http://192.168.1.70/wxapi.php/MobileMonitor/getSubject/subject_id/' + theSubjectID)
        .then((response) => {
          // 获取最大页码数量...
          that.maxGalPage = response.data.maxGalPage
          // 设置 tab 标签数据 => 科目列表...
          that.arrSubject = response.data.arrSubject
          // 动态修改 tab 宽度 => length * 60px => 必须放在 tab 对象上...
          that.tabWidth = that.arrSubject.length * 60
          // 设置 swiper 标签数据 => 最新5个录像文件...
          that.arrNewVod = response.data.arrNewVod
          // 设置 gallery 标签数据 => 最新10个录像文件...
          that.arrGallery = response.data.arrGallery
          // swiper数据不是数组，不显示swiper对象...
          if (!(that.arrNewVod instanceof Array)) {
            that.isDispSwiper = false
            that.arrNewVod = []
          }
          // gallery数据不是数组，不显示gallery对象...
          if (!(that.arrGallery instanceof Array)) {
            that.isDispGallery = false
            that.isDispEnd = true
            that.isLoading = false
            that.arrGallery = []
            // 异步重置上拉滚动框...
            that.$nextTick(() => {
              // 科目宽度改变，重置科目滚动框...
              that.$refs.tabScroller.reset()
              // 没有数据，重置，隐藏...
              let theGalScroll = that.$refs.galScroller
              theGalScroll.$emit('pullup:reset', theGalScroll.uuid)
              theGalScroll.$emit('pullup:disable', theGalScroll.uuid)
            })
            // 如果swiper与gallery都为空 => 修改结束条样式...
            if (!that.isDispSwiper) {
              that.$refs.endScroll.style.lineHeight = '80px'
            }
            // 直接中断返回...
            return
          }
          // 增加页码 => 当前页码小于或等于最大页码时...
          if (that.curGalPage <= that.maxGalPage) {
            ++that.curGalPage
          }
          // 关闭加载框...
          that.isLoading = false
          // 重置滚动框...
          that.$nextTick(() => {
            // 宽度改变，重置科目滚动框...
            that.$refs.tabScroller.reset()
            // 高度改变，重置上拉滚动框，这里需要enable或disable...
            let theScroller = that.$refs.galScroller
            let theOperate = (that.curGalPage > that.maxGalPage) ? 'pullup:disable' : 'pullup:enable'
            that.isDispEnd = (that.curGalPage > that.maxGalPage) ? 1 : 0
            theScroller.$emit('pullup:reset', theScroller.uuid)
            theScroller.$emit(theOperate, theScroller.uuid)
          })
        })
        .catch((error) => {
          that.isLoading = false
          console.log(error)
        })
    }
  },
  mounted () {
    // 默认加载最新的数据...
    this.loadSubject(-1)
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
/* height 必须设定80px */
.endScroll {
  height: 80px;
  color: #999999;
  font-size: 16px;
  text-align: center;
}
</style>
