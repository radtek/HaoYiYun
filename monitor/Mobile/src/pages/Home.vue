<template>
  <div ref="divHome">
    <!-- 这是页面内部切换时需要的等待框 -->
    <loading :show="isLoading" position="absolute" text="加载中"></loading>
    <!-- 需要打开 fastclick 否则 scroller 不能点击 -->
    <!-- vux-tab-item 设置了固定的高度 44px -->
    <scroller lock-y :scrollbar-x=false ref="tabScroller" class="tabGround">
      <tab :line-width="2" active-color="#fc378c" :animate="true" :style="'width:' + tabWidth + 'px'" @on-index-change="onSubjectChange">
        <tab-item :selected="index === 0" v-for="(item, index) in arrGather" :key="index">{{item.name_set}}</tab-item>
      </tab>
    </scroller>
    <scroller lock-x :scrollbar-y=true enable-horizontal-swiping use-pulldown :pulldown-config="pulldownConfig" @pulldown:loading="refresh" use-pullup :pullup-config="pullupConfig" @pullup:loading="loadMore" ref="galScroller">
      <div><!-- 必须包含这个div容器，否则scroller无法拖动 -->
        <swiper v-show="isDispSwiper" :list="arrSwiper" auto loop :threshold=10 height="200px" @on-click-list-item="onClickSwiper"></swiper>
        <galleray v-show="isDispGallery" :isLive="isLive" :list="arrGallery" @on-click-list-item="onClickGallery"></galleray>
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
import Galleray from '@/components/galleray'

/* import { mapState } from 'vuex'
computed: {
  ...mapState({
    fastClick: state => state.vux.fastClick
  })
} */

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
      isDispGallery: true,
      isDispSwiper: true,
      isDispEnd: false,
      isLoading: true,
      isLive: false,
      curSubject: 0,
      curGalPage: 1,
      maxGalPage: 1,
      tabWidth: 300,
      arrGather: [],
      arrSwiper: [],
      arrGallery: [],
      pulldownConfig: {
        height: 30,
        content: '下拉刷新',
        downContent: '下拉刷新',
        upContent: '释放刷新',
        loadingContent: '加载中...'
      },
      pullupConfig: {
        pullUpHeight: 74, // 30 + 44 => 需要对上拉滚动的参数进行偏移修正 => 由于tabBanner的存在...
        height: 74,       // 30 + 44 => 需要对上拉滚动的参数进行偏移修正 => 由于tabBanner的存在...
        content: '上拉加载更多',
        downContent: '松开进行加载',
        upContent: '上拉加载更多',
        loadingContent: '加载中...'
      }
    }
  },
  methods: {
    onSubjectChange (selectIndex) {
      console.log('Subject(%s, %s)', this.arrGather[selectIndex].gather_id, this.arrGather[selectIndex].name_set)
      // 如果当前索引与选中的索引不一样，才更新数据...
      if (this.curSubject !== selectIndex) {
        // 保存当前的科目索引编号 => 并不是数据库编号...
        this.curSubject = selectIndex
        // 获取选定的科目内容...
        this.loadGather(this.arrGather[selectIndex].gather_id)
      }
    },
    onClickSwiper (item) {
      // 只有第一栏是录像点播...
      if (this.curSubject > 0) {
        this.doLivePage(item)
        console.log('Swiper(Live, %s)', item.camera_id)
      } else {
        this.$router.push({name: 'Vod', params: item})
        console.log('Swiper(Vod, %s)', item.record_id)
      }
    },
    onClickGallery (item) {
      // 只有第一栏是录像点播...
      if (this.curSubject > 0) {
        this.doLivePage(item)
        console.log('Gallery(Live, %s)', item.camera_id)
      } else {
        // 根据路由名称跳转 => 不能在这里累加计数，因为页面不会重新加载...
        this.$router.push({name: 'Vod', params: item})
        console.log('Gallery(Vod, %s)', item.record_id)
      }
    },
    doLivePage (item) {
      // 组合需要远程访问的地址...
      let theUrl = '/wxapi.php/Mobile/getHlsAddr/camera_id/' + item.camera_id
      let that = this
      // 设置等待状态...
      that.isLoading = true
      that.$root.$http.get(theUrl)
        .then((response) => {
          // 打印返回信息，关闭等待框...
          that.isLoading = false
          console.log('=== get hls address ===')
          console.log(response.data.hls_url)
          // 判断返回的hls地址是否有效...
          item.arrHlsAddr = response.data
          // 根据路由名称跳转 => 不能在这里累加计数，因为页面不会重新加载...
          this.$router.push({name: 'Live', params: item})
        })
        .catch((error) => {
          that.isLoading = false
          console.log(error)
        })
    },
    refresh (theScroller) {
      let that = this
      let theGatherID = that.arrGather[that.curSubject].gather_id
      let theUrl = '/wxapi.php/Mobile/getSwiper/gather_id/' + theGatherID
      // 从服务器获取选中科目的最新5条记录数据...
      that.$root.$http.get(theUrl)
        .then((response) => {
          // 设置 swiper 标签数据 => 最新5个录像文件...
          that.arrSwiper = response.data
          // swiper数据不是数组，不显示swiper对象...
          that.isDispSwiper = true
          if (!(that.arrSwiper instanceof Array)) {
            that.isDispSwiper = false
            that.arrSwiper = []
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
      let theGatherID = that.arrGather[that.curSubject].gather_id
      let theUrl = '/wxapi.php/Mobile/getGallery/p/' + that.curGalPage + '/gather_id/' + theGatherID
      that.$root.$http.get(theUrl)
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
    loadGather (theGatherID) {
      let that = this
      that.curGalPage = 1
      that.maxGalPage = 1
      that.arrSwiper = []
      that.arrGallery = []
      that.isLoading = true
      that.isDispEnd = false
      that.isDispSwiper = true
      that.isDispGallery = true
      that.$refs.endScroll.style.lineHeight = ''
      that.isLive = (theGatherID !== -1)
      // 这里必须让下拉滚动容器返回到顶部...
      that.$refs.galScroller._xscroll.scrollTop(0, 1000, 'ease-in-out')
      // 向服务器请求科目数据内容...
      let theUrl = '/wxapi.php/Mobile/getGather/gather_id/' + theGatherID
      that.$root.$http.get(theUrl)
        .then((response) => {
          // 获取最大页码数量...
          that.maxGalPage = response.data.maxGalPage
          // 设置 tab 标签数据 => 采集端列表...
          that.arrGather = response.data.arrGather
          // 动态修改 tab 宽度 => length * 70px => 必须放在 tab 对象上...
          that.tabWidth = that.arrGather.length * 70
          // 设置 swiper 标签数据 => 最新5个录像文件...
          that.arrSwiper = response.data.arrSwiper
          // 设置 gallery 标签数据 => 最新10个录像文件...
          that.arrGallery = response.data.arrGallery
          // swiper数据不是数组，不显示swiper对象...
          if (!(that.arrSwiper instanceof Array)) {
            that.isDispSwiper = false
            that.arrSwiper = []
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
    // 设置默认的标题栏名称...
    // document.title = '云监控'
    // 设置最后的滚动结束条的高度...
    this.$refs.endScroll.style.height = '80px'
    // 默认加载最新的数据...
    this.loadGather(-1)
    // 进行局部对象定向绑定...
    this.$store.state.vux.fastClick.attach(this.$refs.divHome)
  },
  created () {
    console.log('home - created')
  },
  activated () {
    console.log('home - activated')
    // 这里由于使用了全局等待，需要停止等待框的显示...
    this.$store.commit('updateLoadingStatus', {isLoading: false})
  },
  deactivated () {
    console.log('home - deactivated')
  }
}
</script>
<style lang="less">
.tabGround {
  background: linear-gradient(180deg, #e5e5e5, #e5e5e5, rgba(229, 229, 229, 0)) bottom left no-repeat;
  background-size: 100% 1px;
  text-align: -webkit-center;
}
</style>
