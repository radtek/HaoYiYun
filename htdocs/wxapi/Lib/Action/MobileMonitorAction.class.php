<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class MobileMonitorAction extends Action
{
  public function _initialize() {
    // 创建一个新的移动检测对象...
    /*$this->m_detect = new Mobile_Detect();
    //////////////////////////////////////////////
    // 电脑端访问了移动端页面，直接对页面进行跳转...
    //////////////////////////////////////////////
    if( !$this->m_detect->isMobile() ) {
      header("location:".__APP__.'/Home/index');
    }*/
  }
  /**
  +----------------------------------------------------------
  * 默认操作，进行页面分发...
  +----------------------------------------------------------
  */
  public function index()
  {
    echo "Mobile-Cloud-Monitor";
  }
  //
  // 获取最大页码...
  private function fetchMaxPage()
  {
    // 通过传递过来的科目编号进行数据分发...
    $theSubjectID = $_GET['subject_id'];
    switch( $theSubjectID ) {
      case -1: $map['Record.record_id'] = array('gt', 0); break;
      case -2: $map['Camera.camera_id'] = array('gt', 0); break;
      default: $map['Record.subject_id'] = $theSubjectID; break;
    }
    // 获取每页记录数...
    $pagePer = C('PAGE_PER');
    if( $theSubjectID == -2 ) {
      $totalNum = D('LiveView')->where($map)->count();
    } else {
      $totalNum = D('RecordView')->where($map)->count();
    }
    // 计算总页数...
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    return $max_page;
  }
  //
  // 获取最新swiper接口...
  private function fetchArrSwiper()
  {
    // 通过传递过来的科目编号进行数据分发...
    $theSubjectID = $_GET['subject_id'];
    switch( $theSubjectID ) {
      case -1: $map['Record.record_id'] = array('gt', 0); break;
      case -2: $map['Camera.camera_id'] = array('gt', 0); break;
      default: $map['Record.subject_id'] = $theSubjectID; break;
    }
    // 筛选出最新的5个直播或点播节目...
    if( $theSubjectID == -2 ) {
      // 获取最新的5个直播节目...
      $arrSwiper = D('LiveView')->where($map)->limit(5)->order('Camera.status DESC, Camera.created DESC')->select();
      // 获取服务器地址 => http 或 https
      $strAddr = "http://";
      if((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on') || (isset($_SERVER['REQUEST_SCHEME']) && $_SERVER['REQUEST_SCHEME'] == 'https')) {
        $strAddr = "https://";
      }
      // 重新组合swiper需要的数据内容 => 在线优先...
      foreach($arrSwiper as $key => &$dbItem) {
        //$strLiveImg = (($dbItem['status'] <= 0) ? "wxapi/public/images/live-off.png" : "wxapi/public/images/live-on.png");
        //$theImgUrl = sprintf("%s%s:%d/%s", $strAddr, $_SERVER['HTTP_HOST'], $_SERVER['SERVER_PORT'], $strLiveImg);
        //$theTitle = sprintf("%s %s %s", $dbItem['grade_type'], $dbItem['grade_name'], $dbItem['camera_name']);
        //$arrNewVod[$key] = array('id' => $dbItem['camera_id'], 'url' => 'javascript:', 'img' => $theImgUrl, 'title' => $theTitle);
        $strLiveImg = (($dbItem['status'] <= 0) ? "wxapi/public/images/live-off.png" : "wxapi/public/images/live-on.png");
        $dbItem['img'] = sprintf("%s%s:%d/%s", $strAddr, $_SERVER['HTTP_HOST'], $_SERVER['SERVER_PORT'], $strLiveImg);
        $dbItem['title'] = sprintf("%s %s %s", $dbItem['grade_type'], $dbItem['grade_name'], $dbItem['camera_name']);
      }
    } else {
      // 获取最新的5个点播节目...
      $arrSwiper = D('RecordView')->where($map)->limit(5)->order('Record.created DESC')->select();
      // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
      $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
      // 重新组合swiper需要的数据内容...
      foreach($arrSwiper as $key => &$dbItem) {
        //$theImgUrl = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
        //$theTitle = sprintf("%s %s %s %s", $dbItem['subject_name'], $dbItem['grade_type'], $dbItem['teacher_name'], $dbItem['title_name']);
        //$arrNewVod[$key] = array('id' => $dbItem['record_id'], 'url' => 'javascript:', 'img' => $theImgUrl, 'title' => $theTitle);
        $dbItem['img'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
        $dbItem['title'] = sprintf("%s %s %s %s", $dbItem['subject_name'], $dbItem['grade_type'], $dbItem['teacher_name'], $dbItem['title_name']);
        $dbItem['image_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
        $dbItem['file_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['file_fdfs']);
      }
    }
    return $arrSwiper;
  }
  //
  // 获取分页gallery接口...
  private function fetchArrGallery()
  {
    // 通过传递过来的科目编号进行数据分发...
    $theSubjectID = $_GET['subject_id'];
    switch( $theSubjectID ) {
      case -1: $map['Record.record_id'] = array('gt', 0); break;
      case -2: $map['Camera.camera_id'] = array('gt', 0); break;
      default: $map['Record.subject_id'] = $theSubjectID; break;
    }
    // 得到每页条数...
    $pagePer = C('PAGE_PER');
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取直播或点播分页记录数据...
    if( $theSubjectID == -2 ) {
      // 获取服务器地址...
      $strAddr = "http://";
      if((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on') || (isset($_SERVER['REQUEST_SCHEME']) && $_SERVER['REQUEST_SCHEME'] == 'https')) {
        $strAddr = "https://";
      }
      // 获取直播分页数据，并对数据进行重新组合...
      $arrGallery = D('LiveView')->where($map)->limit($pageLimit)->order('Camera.status DESC, Camera.created DESC')->select();
      foreach($arrGallery as &$dbItem) {
        $strLiveImg = (($dbItem['status'] <= 0) ? "wxapi/public/images/live-off.png" : "wxapi/public/images/live-on.png");
        $dbItem['image_fdfs'] = sprintf("%s%s:%d/%s", $strAddr, $_SERVER['HTTP_HOST'], $_SERVER['SERVER_PORT'], $strLiveImg);
        $dbItem['subject_name'] = $dbItem['grade_name'];
        $dbItem['teacher_name'] = $dbItem['camera_name'];
        $dbItem['title_name'] = $dbItem['school_name'];
      }
    } else {
      // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
      $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
      // 获取点播分页数据，并对缩略图片进行地址重组...
      $arrGallery = D('RecordView')->where($map)->limit($pageLimit)->order('Record.created DESC')->select();
      // 组合需要返回的数据 => web_tracker_addr 已经自带了协议头 http://或https://
      foreach($arrGallery as &$dbItem) {
        $dbItem['image_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
        $dbItem['file_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['file_fdfs']);
      }
    }
    return $arrGallery;
  }
  //
  // 得到科目列表...
  // 参数：subject_id => 科目编号...
  public function getSubject()
  {
    // 指定其它域名访问内容 => 跨域访问...
    //header('Access-Control-Allow-Origin:*');
    // 筛选出所有的科目列表，在数组头部加入两个元素...
    $arrSubject = D('subject')->field('subject_id,subject_name')->order('subject_id ASC')->select();
    array_unshift($arrSubject, array('subject_id' => -1, 'subject_name' => '最新'), array('subject_id' => -2, 'subject_name' => '直播'));
    // 组合需要的返回数据块...
    $arrData['maxGalPage'] = $this->fetchMaxPage();
    $arrData['arrGallery'] = $this->fetchArrGallery();
    $arrData['arrSwiper'] = $this->fetchArrSwiper();
    $arrData['arrSubject'] = $arrSubject;
    // 返回json编码数据包...
    echo json_encode($arrData);
  }
  //
  // 得到分页记录...
  // 参数：subject_id => 科目编号
  public function getGallery()
  {
    // 指定其它域名访问内容 => 跨域访问...
    //header('Access-Control-Allow-Origin:*');
    // 通过接口函数获取分页数据...
    $arrGallery = $this->fetchArrGallery();
    // 返回json编码数据包...
    echo json_encode($arrGallery);
  }
  //
  // 得到swiper数据内容 => 5 条最新数据...
  // 参数：subject_id => 科目编号
  public function getSwiper()
  {
    // 指定其它域名访问内容 => 跨域访问...
    //header('Access-Control-Allow-Origin:*');
    // 通过接口函数获取分页数据...
    $arrSwiper = $this->fetchArrSwiper();
    // 返回json编码数据包...
    echo json_encode($arrSwiper);
  }
  //
  // 获取与直播通道相关的录像记录...
  public function getRecord()
  {
    // 指定其它域名访问内容 => 跨域访问...
    //header('Access-Control-Allow-Origin:*');
    // 得到每页条数...
    $pagePer = C('PAGE_PER');
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
    $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
    // 获取点播分页数据，并对缩略图片进行地址重组...
    $map['camera_id'] = $_GET['camera_id'];
    $arrRecord = D('RecordView')->where($map)->limit($pageLimit)->order('Record.created DESC')->select();
    // 组合需要返回的数据 => web_tracker_addr 已经自带了协议头 http://或https://
    foreach($arrRecord as &$dbItem) {
      $dbItem['image_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
      $dbItem['file_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['file_fdfs']);
    }
    // 返回json编码数据包...
    echo json_encode($arrRecord);
  }
  //
  // 保存点击次数...
  public function saveClick()
  {
    // 指定其它域名访问内容 => 跨域访问...
    //header('Access-Control-Allow-Origin:*');
    // 点播和直播的点击次数分开处理...
    if( strcasecmp($_GET['type'], "vod") == 0 ) {
      $map['record_id'] = $_GET['record_id'];
      $dbVod = D('record')->where($map)->field('clicks')->find();
      // 累加点击计数器，写入数据库...
      $dbSave['clicks'] = intval($dbVod['clicks']) + 1;
      $dbSave['record_id'] = $_GET['record_id'];
      D('record')->save($dbSave);
      // 返回计数结果...
      echo $dbSave['clicks'];
    } else if( strcasecmp($_GET['type'], "live") == 0 ) {
      $map['camera_id'] = $_GET['camera_id'];
      $dbLive = D('camera')->where($map)->field('clicks')->find();
      // 累加点击计数器，写入数据库...
      $dbSave['clicks'] = intval($dbLive['clicks']) + 1;
      $dbSave['camera_id'] = $_GET['camera_id'];
      D('camera')->save($dbSave);
      // 返回计数结果...
      echo $dbSave['clicks'];
    }
  }
  //
  // 获取直播地址...
  public function getHlsAddr()
  {
    // 指定其它域名访问内容 => 跨域访问...
    //header('Access-Control-Allow-Origin:*');
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    $arrErr['err_desc'] = "OK";
    // 进行处理过程，有错误就返回...
    do {
      // 首先，判断通道是否处于直播状态...
      $map['camera_id'] = $_GET['camera_id'];
      $dbCamera = D('LiveView')->where($map)->field('camera_id,clicks,status,gather_id,mac_addr')->find();
      // 如果通道不在线，直接返回错误...
      if( $dbCamera['status'] <= 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '当前通道处于离线状态，无法播放！';
        $arrErr['err_desc'] = '请联系管理员，开启通道。';
        break;
      }
      // 中转服务器需要的参数...
      $dbParam['mac_addr'] = $dbCamera['mac_addr'];
      $dbParam['rtmp_live'] = $dbCamera['camera_id'];
      // 获取直播链接地址...
      $dbResult = $this->getRtmpUrlFromTransmit($dbParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = $dbResult['err_msg'];
        $arrErr['err_desc'] = '请联系管理员，汇报错误信息。';
        break;
      }
      // 将反馈结果进行重新组合...
      $arrErr['hls_url'] = $dbResult['hls_url'];
      $arrErr['hls_type'] = $dbResult['hls_type'];
      // 这3个参数是直播播放器汇报时需要的数据...
      $arrErr['player_camera'] = $dbCamera['camera_id'];
      $arrErr['player_id'] = $dbResult['player_id'];
      $arrErr['player_vod'] = 0;
    }while( false );
    // 反馈结果信息...
    echo json_encode($arrErr);
  }
  //
  // 2017.06.15 - by jackey => 修改了中转服务器代码，转发命令给采集端之后，立即返回rtmp地址，无需等待采集端上传结果，避免阻塞，让播放器自己去等待...
  // 从中转服务器获取直播地址...
  // 返回一个数组结果...
  private function getRtmpUrlFromTransmit(&$dbParam)
  {
    // 尝试链接中转服务器...
    $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
    
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
    // 链接中转服务器失败，直接返回...
    if( !$transmit ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '无法连接中转服务器。';
      return $arrData;
    }
    // 获取直播频道所在的URL地址...
    $saveJson = json_encode($dbParam);
    $json_data = transmit_command(kClientPlay, kCmd_Play_Login, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 获取的JSON数据有效，转成数组，直接返回...
    $arrData = json_decode($json_data, true);
    if( !$arrData ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '从中转服务器获取数据失败。';
      return $arrData;
    }
    // 通过错误码，获得错误信息...
    $arrData['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    // 将整个数组返回...
    return $arrData;
  }
}
?>