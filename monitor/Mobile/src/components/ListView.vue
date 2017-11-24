<template>
  <div>
      <div class="list_media_box" v-for="(item, index) in list" @click="clickListItem(item)">
        <div class="list_media_hd"><!-- :style="'background: url(' + boxGround + ') no-repeat center;'" -->
          <div class="c-time">
            <div class="thumb_back"></div>
            <div class="thumb_time">{{item.disptime}}</div>
          </div>
          <img class="list_media_thumb" v-lazy="item.image_fdfs + '_240x140'" />
        </div>
        <div class="list_media_bd">
          <h4 class="list_media_title" style="line-height:25px;" :style="{color: (item.record_id===focusRecord) ? '#FF5722' : '#000'}">
            <i class="fa fa-file-video-o">&nbsp;{{item.camera_name}}</i>
          </h4>
          <div class="list_media_desc" style="font-size:14px;">
            <span><i class="fa fa-clock-o">&nbsp;{{item.created}}</i></span>
            <span style="float: right;"><i class="fa fa-play-circle-o">&nbsp;{{item.clicks}}次</i></span>
          </div>
        </div>
      </div>
   </div>
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
    focusRecord: {
      type: String,
      default: ''
    }
    /* boxGround: {
      type: String,
      default: ''
    }, */
  },
  methods: {
    clickListItem (item) {
      // 当前对象与已经选中的不是一样的，通知父组件...
      if (item.record_id !== this.focusRecord) {
        // 向上层应用传递点击事件消息通知 => 不要转换成json => 这样数据就能自动同步...
        this.$emit('on-click-list-item', item)
      }
    }
  },
  mounted () {
  }
}
</script>

<style lang="less">
.c-time {
  position: absolute;
  color: #FFF;
  height: 16px;
  line-height: 16px;
  bottom: 2px;
  right: 2px;
}
.thumb_back {
  height: 16px;
  line-height: 16px;
  width: 100%;
  opacity: 0.5;
  background: #000;
  border-radius: 2px;
  position: absolute;
}
.thumb_time {
  position: relative;
  font-size: 14px;
  padding: 0 3px;
}
.list_media_box {
  color: #000;
  margin: 0px 12px;
  padding: 10px 0px;
  position: relative;
  display: flex;
  display: -ms-flexbox;
  align-items: center;
  -ms-flex-align: center;
  -webkit-tap-highlight-color: rgba(0,0,0,0);
  border-bottom: 1px solid #ddd;
}
.list_media_hd {
  width: 120px;
  height: 70px;
  text-align: center;
  display: flex;
  position: relative;
  margin-right: 10px;
}
.list_media_bd {
  flex: 1;
  -ms-flex: 1;
  min-width: 0;
}
.list_media_thumb {
  width: 120px;
  height: 100%;
  margin: auto;
}
.list_media_title {
  font-weight: 400;
  font-size: 15px;
  height: 54px;
  line-height: 18px;
  overflow:hidden; 
  text-overflow:ellipsis;
  display:-webkit-box; 
  -webkit-box-orient:vertical;
  -webkit-line-clamp:3;
}
.list_media_desc {
  color: #888;
  height: 16px;
  line-height: 16px;
  overflow: hidden;
  font-size: 12px;
  margin-top: 2px;
}
</style>
