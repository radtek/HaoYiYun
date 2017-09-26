<template>
  <ul class="am-gallery am-avg-sm-2 am-gallery-bordered">
    <li v-for="(item, index) in list" @click="clickListItem(item)">
      <div class="am-gallery-item">
        <a href="javascript:">
          <div class="am-gallery-box" :style="'background: url(../../static/' + boxGround + ') no-repeat center;'">
            <div class="am-gallery-play" :style="{display: isLive ? 'none' : 'block'}"><i class="fa fa-play-circle"></i></div>
            <img class="am-gallery-img" v-lazy="item.image_fdfs + '_240x140'" />
          </div>
          <h3 class="am-gallery-title">{{item.subject_name}} {{item.grade_type}} {{item.teacher_name}} {{item.title_name}}</h3>
          <div class="am-gallery-desc">{{item.created}}</div>
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
    boxGround: {
      type: String,
      default: ''
    },
    isLive: {
      type: Boolean,
      default: false
    }
    /* webTracker: {
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
  color: #999999;
}
</style>
