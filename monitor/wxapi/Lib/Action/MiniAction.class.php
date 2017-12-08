<?php
/*************************************************************
    HaoYi (C)2017 - 2018 myhaoyi.com
    备注：专门处理微信小程序从中心服务器转发的请求命令...
*************************************************************/

class MiniAction extends Action
{
  public function _initialize() {
  }
  // 获取共享通道记录列表...
  public function getShare()
  {
    // 准备返回结果状态...
    $arrErr['errcode'] = false;
    $arrErr['errmsg'] = 'ok';
    do {
      // 判断输入参数是否有效...
      if( !isset($_GET['list']) ) {
        $arrErr['errcode'] = true;
        $arrErr['errmsg'] = '输入参数无效！';
        break;
      }
      // 查询通道记录...
      $condition['camera_id'] = array('in', $_GET['list']);
      $arrErr['track'] = D('LiveView')->where($condition)->field('camera_id,stream_prop,status,clicks,camera_name,image_fdfs')->select();
    }while( false );
    // 直接反馈查询结果...
    echo json_encode($arrErr);
  }
}
?>