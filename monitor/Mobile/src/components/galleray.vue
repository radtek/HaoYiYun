<template>
  <ul class="am-gallery am-avg-sm-2 am-gallery-bordered">
    <li v-for="(item, index) in list" @click="clickListItem(item)">
      <div class="am-gallery-item">
        <a href="javascript:">
          <div class="am-gallery-box"><!--:style="'background: url(' + boxGround + ') no-repeat center;'"-->
            <div class="am-gallery-play" :style="{display: isLive ? 'none' : 'block'}">
              <i class="fa fa-play-circle"></i>
            </div>
            <!-- 直播通道状态 -->
            <template v-if="isLive">
              <div class="am-gallery-up">
              <template v-if="item.status >= 1">
                <div class="am-thumb-back online"></div>
                <div class="am-thumb-text">直播中</div>
              </template>
              <template v-else>
                <div class="am-thumb-back offline"></div>
                <div class="am-thumb-text">离线中</div>
              </template>
              </div>
            </template>
            <!-- 录像播放时长 -->
            <template v-else>
              <div class="am-gallery-down">
                <div class="am-thumb-back dtime"></div>
                <div class="am-thumb-text">{{item.disptime}}</div>
              </div>
            </template>
            <img class="am-gallery-img" v-lazy="item.image_fdfs + '_240x140'" />
          </div>
          <h3 class="am-gallery-title">
            <template v-if="isLive">
              <template v-if="item.stream_prop == 0">
                <i class="fa fa-camera">&nbsp;摄像头 - {{item.camera_name}}</i>
              </template>
              <template v-else-if="item.stream_prop == 1">
                <i class="fa fa-file-video-o">&nbsp;MP4文件 - {{item.camera_name}}</i>
              </template>
              <template v-else-if="item.stream_prop == 2">
                <i class="fa fa-arrow-circle-o-down">&nbsp;流转发 - {{item.camera_name}}</i>
              </template>
            </template>
            <template v-else>
              <i class="fa fa-clock-o">&nbsp;{{item.created}}</i>
            </template>
          </h3>
          <!-- 录像记录需要的额外信息 -->
          <template v-if="isLive === false">
          <div class="am-gallery-desc">
            <span class="am-left">
              <i class="fa fa-file-video-o">&nbsp;{{item.file_size}}&nbsp;MB</i>
            </span>
            <span class="am-right">
              <i class="fa fa-play-circle-o">&nbsp;{{item.clicks}}&nbsp;次</i>
            </span>
          </div>
          </template>
        </a>
      </div>
    </li>
  </ul>
</template>

<script>
export default {
  props: {
    list: {
      type: Array,
      default () {
        return []
      }
    },
    isLive: {
      type: Boolean,
      default: false
    }
    /* boxGround: {
      type: String,
      default: ''
    },
    webTracker: {
      type: String,
      default: 'http://'
    } */
  },
  data () {
    return {
      // defErrImg: 'this.src="' + require('../assets/blank.gif') + '"'
      lastSelElem: null
    }
  },
  methods: {
    clickListItem (item) {
      // 上次选中对象有效，把它的样式还原...
      if (this.lastSelElem) {
        this.lastSelElem.setAttribute('class', 'am-gallery-item')
      }
      // 获取当前选中对象，给它设置新的焦点样式...
      this.lastSelElem = event.currentTarget.firstElementChild
      this.lastSelElem.setAttribute('class', 'am-gallery-focus')
      // 向上层应用传递点击事件消息通知...
      this.$emit('on-click-list-item', JSON.parse(JSON.stringify(item)))
    }
  },
  mounted () {
    // this.$Lazyload.$on('loaded', function ({ el, src }) {
    //  console.log(el, src)
    // })
  }
}
</script>

<style lang="less">

@import '~font-awesome/less/font-awesome.less';

/* ==========================================================================
   amaze gallery image
 ============================================================================ */
a {
  color: #0e90d2;
  text-decoration: none;
  background-color: transparent;
}
/* ==========================================================================
   Component: AVG Grid
 ============================================================================ */
[class*="am-avg-"] {
  display: block;
  padding: 0;
  margin: 0;
  list-style: none;
}
[class*="am-avg-"]:before,
[class*="am-avg-"]:after {
  content: " ";
  display: table;
}
[class*="am-avg-"]:after {
  clear: both;
}
[class*="am-avg-"] > li {
  display: block;
  height: auto;
  float: left;
}

.am-left {
  margin-left: 2px;
}
.am-right {
  float: right;
  margin-right: 5px;
}

/* @media only screen */
.am-avg-sm-2 > li {
  width: 50%;
}
.am-avg-sm-2 > li:nth-of-type(n) {
  clear: none;
}
.am-avg-sm-2 > li:nth-of-type(2n + 1) {
  clear: both;
}

.am-gallery {
  font-size: 12px;
  padding: 5px 5px 5px 5px;
  list-style: none;
}
.am-gallery h3 {
  margin: 0;
}
.am-gallery-box {
  width: 100%;
  height: 95px;
  line-height: 95px;
  overflow: hidden;
  text-align: center;
  position: relative;
  display: flex;
}
.am-gallery-img {
  vertical-align: middle;
  display: inline-block;
  border: 0;
}
.am-gallery-play {
  width: 100%;
  color: #fff;
  opacity: 0.8;
  font-size: 36px;
  position: absolute;
  text-align: center;
  /* transform: translateY(-50%);
  top: 45%; */
}
.am-gallery-up {
  position: absolute;
  color: #FFF;
  height: 18px;
  line-height: 18px;
  right: 5px;
  top: 5px;
}
.am-gallery-down {
  position: absolute;
  color: #FFF;
  height: 18px;
  line-height: 18px;
  bottom: 2px;
  right: 2px;
}
.am-thumb-back {
  width: 100%;
  height: 18px;
  line-height: 18px;
  border-radius: 2px;
  position: absolute;
}
.am-thumb-text {
  position: relative;
  margin: 0px 3px 0px 3px;
}
.dtime {
  opacity: 0.5;
  background: #000;
}
.online {
  background: #ff6d34;
}
.offline {
  background: #5aa700;
}
/**
  * Gallery Theme: default
  * Author: Minwe (minwe@yunshipei.com)
  */
.am-gallery-default > li {
  padding: 5px;
  -webkit-box-sizing: border-box;
          box-sizing: border-box;
}
.am-gallery-default .am-gallery-item img {
  width: 100%;
  height: auto;
}
.am-gallery-default .am-gallery-title {
  margin-top: 5px;
  font-weight: normal;
  display: block;
  word-wrap: normal;
  /* for IE */
  text-overflow: ellipsis;
  white-space: nowrap;
  overflow: hidden;
  color: #555555;
}
.am-gallery-default .am-gallery-desc {
  color: #999999;
}
/**
  * Gallery Theme: overlay
  * Author: Minwe (minwe@yunshipei.com)
  */
.am-gallery-overlay > li {
  padding: 5px;
  -webkit-box-sizing: border-box;
          box-sizing: border-box;
}
.am-gallery-overlay .am-gallery-item {
  position: relative;
}
.am-gallery-overlay .am-gallery-item img {
  width: 100%;
  height: auto;
}
.am-gallery-overlay .am-gallery-title {
  font-weight: normal;
  color: #FFF;
  position: absolute;
  bottom: 0;
  width: 100%;
  background-color: rgba(0, 0, 0, 0.5);
  text-indent: 5px;
  height: 30px;
  line-height: 30px;
  display: block;
  word-wrap: normal;
  /* for IE */
  text-overflow: ellipsis;
  white-space: nowrap;
  overflow: hidden;
}
.am-gallery-overlay .am-gallery-desc {
  display: none;
}
/**
  * Accordion Theme: bordered
  * Author: Minwe (minwe@yunshipei.com)
  */
.am-gallery-bordered > li {
  padding: 5px;
  -webkit-box-sizing: border-box;
          box-sizing: border-box;
}
.am-gallery-bordered .am-gallery-item {
  -webkit-box-shadow: 0 0 3px rgba(0, 0, 0, 0.35);
          box-shadow: 0 0 3px rgba(0, 0, 0, 0.35);
  padding: 5px;
}
.am-gallery-bordered .am-gallery-focus {
  border: 1px solid #FF5722;
  padding: 4px;
}
.am-gallery-bordered img {
  width: 100%;
  height: auto;
}
.am-gallery-bordered .am-gallery-title {
  margin-left: 2px;
  margin-top: 5px;
  font-weight: normal;
  color: #555555;
  display: block;
  word-wrap: normal;
  /* for IE */
  text-overflow: ellipsis;
  white-space: nowrap;
  overflow: hidden;
}
.am-gallery-bordered .am-gallery-desc {
  font-size: 13px;
  color: #888;
}
</style>
