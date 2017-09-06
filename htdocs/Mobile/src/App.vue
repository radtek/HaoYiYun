<template>
  <div>
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
        <swiper :list="arrNewVod" auto loop :threshold=10 height="200px" @on-click-list-item="onClickSwiper"></swiper>
        <galleray :webTracker="webTracker" :list="arrGallery" @on-click-list-item="onClickGallery"></galleray>
        <divider v-show="isDispEnd">没有更多内容了</divider>
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
import Divider from 'vuxx-components/divider'

import Galleray from './components/galleray'

export default {
  components: {
    Tab,
    TabItem,
    Swiper,
    Loading,
    Scroller,
    Galleray,
    Divider
  },
  data () {
    return {
      isDispEnd: false,
      isLoading: true,
      curGalPage: 1,
      maxGalPage: 1,
      tabWidth: 300,
      arrSubject: [],
      arrNewVod: [],
      arrGallery: [],
      webTracker: '',
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
    onSubjectChange (currentIndex) {
      // this.arrSubject[currentIndex].subject_id   => 科目编号...
      // this.arrSubject[currentIndex].subject_name => 科目名称...
      console.log('Suject(' + this.arrSubject[currentIndex].subject_id + ', ' + this.arrSubject[currentIndex].subject_name + ')')
    },
    onClickSwiper (item) {
      console.log('Swiper: ' + item.id)
    },
    onClickGallery (item) {
      console.log('Galleray: ' + item.record_id)
    },
    refresh (theScroller) {
      setTimeout(function () {
        theScroller.$emit('pulldown:reset', theScroller.uuid)
      }, 1000)
    },
    loadMore (theScroller) {
      // 保存当前处理对象...
      let that = this
      // 判断页码是否超过了最大值...
      if (that.curGalPage > that.maxGalPage) {
        // 设置数据读取完毕，停止上拉加载...
        that.$nextTick(() => {
          theScroller.$emit('pullup:done', theScroller.uuid)
          that.isDispEnd = true
        })
      }
      that.$root.$http.get('http://192.168.1.70/wxapi.php/MobileMonitor/getGallery/p/' + that.curGalPage)
        .then((response) => {
          // 叠加记录...
          that.arrGallery = that.arrGallery.concat(response.data)
          // 增加页码...
          ++that.curGalPage
          // 重置上拉条...
          that.$nextTick(() => {
            theScroller.$emit('pullup:reset', theScroller.uuid)
          })
        })
        .catch((error) => {
          console.log(error)
          theScroller.$emit('pullup:reset', theScroller.uuid)
        })
    }
  },
  mounted () {
    let that = this
    that.$root.$http.get('http://192.168.1.70/wxapi.php/MobileMonitor/getSubject')
      .then((response) => {
        // 设置 tab 标签数据 => 科目列表...
        that.arrSubject = response.data.arrSubject
        // 动态修改 tab 宽度 => length * 60px => 必须放在 tab 对象上...
        that.tabWidth = that.arrSubject.length * 60
        // 设置 swiper 标签数据 => 最新5个录像文件...
        that.arrNewVod = response.data.arrNewVod
        // 设置 gallery 标签数据 => 最新10个录像文件...
        that.arrGallery = response.data.arrGallery
        // 获取图片或视频地址前缀...
        that.webTracker = response.data.webTracker
        // 获取最大页码数量...
        that.maxGalPage = response.data.maxGalPage
        // 增加页码...
        ++that.curGalPage
        // 关闭加载框...
        that.isLoading = false
        // 重置滚动框...
        that.$nextTick(() => {
          // 宽度改变，重置科目滚动框...
          that.$refs.tabScroller.reset()
          // 高度改变，重置上拉滚动框...
          let theScroller = that.$refs.galScroller
          theScroller.$emit('pullup:reset', theScroller.uuid)
        })
      })
      .catch((error) => {
        that.isLoading = false
        console.log(error)
      })
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
</style>
