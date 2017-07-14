<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class HomeAction extends Action
{
  public function _initialize()
  {
    // 创建一个新的移动检测对象...
    $this->m_detect = new Mobile_Detect();
    //////////////////////////////////////////////
    // 移动端访问了电脑端页面，直接对页面进行跳转...
    //////////////////////////////////////////////
    if( $this->m_detect->isMobile() ) {
      header("location:".__APP__.'/Mobile/index');
    }
    // 获取系统配置，根据配置设置相关变量...
    $dbSys = D('system')->field('web_type,web_title')->find();
    $this->m_webType = $dbSys['web_type'];
    $this->m_webTitle = $dbSys['web_title'];
    if( $this->m_webType > 0 ) {
      $this->m_webLogo = "monitor";
      $this->m_webName = "云监控";
    } else {
      $this->m_webLogo = "default";
      $this->m_webName = "云录播";
    }
    // 直接给模板变量赋值...
    $this->assign('my_web_logo', $this->m_webLogo);
    $this->assign('my_web_name', $this->m_webName);
  }
  //
  // 点击查看用户登录信息...
  public function userInfo()
  {
    // 判断用户是否已经处于登录状态，直接进行页面跳转...
    if( !Cookie::is_set('wx_unionid') || !Cookie::is_set('wx_headurl') || !Cookie::is_set('wx_ticker') ) {
      echo "<script>window.parent.doReload();</script>";
      return;
    }
    // 调用Admin->userInfo接口...
    A('Admin')->userInfo();
  }
  //
  // 点击注销导航页...
  public function logout()
  {
    ////////////////////////////////////////////////////////////////////
    //注意：不能用Cookie::delete()无法彻底删除，它把空串进行了base64编码...
    ////////////////////////////////////////////////////////////////////

    // 删除存放的用户cookie信息...
    setcookie('wx_unionid','',-1,'/');
    setcookie('wx_headurl','',-1,'/');
    setcookie('wx_ticker','',-1,'/');
    
    $my_nav['is_login'] = false;
    $this->assign('my_nav', $my_nav);
    $this->display('Common:home_login');
  }
  //
  // 点击登录导航页...
  // 参数：wx_error | wx_unionid | wx_headurl
  public function login()
  {
    A('Login')->login(false);
  }
  //
  // 得到导航数据...
  private function getNavData($activeType, $activeID)
  {
    // 根据cookie设定 登录|注销 导航栏...
    if( Cookie::is_set('wx_unionid') && Cookie::is_set('wx_headurl') && Cookie::is_set('wx_ticker') ) {
      $my_nav['is_login'] = true;
      $my_nav['headurl'] = Cookie::get('wx_headurl');
    } else {
      $my_nav['is_login'] = false;
    }
    // 获取科目列表...
    $arrDrop = D('subject')->order('subject_id ASC')->select();
    // 设置一些返回数据...
    $my_nav['type'] = $activeType;          // 0(首页激活), 1(直播激活), 2(科目激活), 3(更多激活)
    $my_nav['active_id'] = $activeID;       // 激活的科目编号...
    $my_nav['drop_more'] = "更多";          // 默认的下拉名称...
    // 如果是科目状态，需要进一步的判断...
    if( $activeType == NAV_ACTIVE_SUBJECT ) {
      // 遍历科目列表，设置下拉名称...
      foreach($arrDrop as &$dbItem) {
        if( $dbItem['subject_id'] == $my_nav['active_id'] ) {
          $my_nav['subject_title'] = $dbItem['subject_name'];
          break;
        }
      }
      // 如果是下拉状态，设置下拉名称...
      if( isset($_GET['more']) ) {
        $my_nav['type'] = NAV_ACTIVE_MORE;
        $my_nav['drop_more'] = $my_nav['subject_title'];
      }
    }
    // 将获取的科目数组进行分割...
    $arrList = array_splice($arrDrop, 0, NAV_ITEM_COUNT);
    // 将分割后的数据进行赋值...
    $my_nav['list'] = $arrList;             // 横向科目列表...
    $my_nav['drop'] = $arrDrop;             // 下拉科目列表...
    $my_nav['drop_num'] = count($arrDrop); // 下拉科目数量...
    // 直接返回计算之后的导航数据...
    return $my_nav;
  }
  /**
  +----------------------------------------------------------
  * 默认操作 => 首页...
  +----------------------------------------------------------
  */
  public function index()
  {
    // 获取首页导航数据...
    $my_nav = $this->getNavData(NAV_ACTIVE_HOME, 0);
    // 合并科目数组，遍历科目数据记录...
    $arrList = array_merge($my_nav['list'], $my_nav['drop']);
    foreach($arrList as &$dbItem) {
      $map['Record.subject_id'] = $dbItem['subject_id'];
      $theName = $dbItem['subject_name'];
      $arrRec[$theName]['id'] = $dbItem['subject_id'];
      $arrRec[$theName]['data'] = D('RecordView')->where($map)->limit(8)->order('record_id DESC')->select();
    }
    //print_r($arrRec); exit;
    // 计算右侧，最近更新，前10条...
    $arrNews = D('RecordView')->limit(10)->order('Record.created DESC')->select();
    $this->assign('my_news', $arrNews);
    //计算右侧，点击排行，前10条...
    $arrList = D('RecordView')->limit(10)->order('Record.clicks DESC')->select();
    $this->assign('my_clicks', $arrList);
    // 对模板对象进行赋值...
    $dbSys = D('system')->field('tracker_addr')->find();
    $this->assign('my_tracker', sprintf("http://%s/", $dbSys['tracker_addr']));
    $this->assign('my_title', $this->m_webTitle . ' - 首页');
    $this->assign('my_rec', $arrRec);
    $this->assign('my_nav', $my_nav);
    $this->display('index');
  }
  /**
  +----------------------------------------------------------
  * 科目页面...
  +----------------------------------------------------------
  */
  public function subject()
  {
    // 获取科目导航数据，如果是下拉状态，设置对应的标志信息...
    $subject_id = $_GET['id'];
    $my_nav = $this->getNavData(NAV_ACTIVE_SUBJECT, $subject_id);
    $my_nav['more'] = ($my_nav['type'] == NAV_ACTIVE_MORE) ? "more/1" : "";
    // 加载分页类...
    import("@.ORG.Page");
    // 设置分页参数...
    $pagePer = 12; // 每页显示的条数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 获取科目编号...
    $map['Record.subject_id'] = $subject_id;
    // 获取记录总条数...
    $totalNum = D('RecordView')->where($map)->count();
    // 进行数据查询操作...
    $arrRec = D('RecordView')->where($map)->limit($pageLimit)->order('record_id DESC')->select();
    $thePage = new Page($totalNum, $pagePer);
    $my_page = $thePage->show();
    // 设置科目需要的模板参数...
    $this->assign('my_rec', $arrRec);
    $this->assign('my_page', $my_page);
    // 计算右侧，最近更新，前10条...
    $where['Record.subject_id'] = $subject_id;
    $arrNews = D('RecordView')->where($where)->limit(10)->order('Record.created DESC')->select();
    $this->assign('my_news', $arrNews);
    // 对模板对象进行赋值...
    $this->assign('my_title', $this->m_webTitle . ' - ' . $my_nav['subject_title']);
    $dbSys = D('system')->field('tracker_addr')->find();
    $this->assign('my_tracker', sprintf("http://%s/", $dbSys['tracker_addr']));
    $this->assign('my_nav', $my_nav);
    $this->display('subject');
  }
  /**
  +----------------------------------------------------------
  * 获取科目分页详情...
  +----------------------------------------------------------
  */
  /*public function getSubject()
  {
    // 获取传递过来的参数信息...
    $theSubject = $_GET['subject_id'];
    $theType = $_GET['grade'];
    // 加载分页类...
    import("@.ORG.Page");
    // 准备分页参数...
    $pagePer = 8;         // 每页条数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 设置年级类型...
    $map['Record.subject_id'] = $theSubject;
    $map['Grade.grade_type'] = $theType;
    // 获取记录总条数...
    $totalNum = D('RecordView')->where($map)->count();
    // 数据有效时，才进行存放处理...
    if( $totalNum > 0 ) {
      $arrList = D('RecordView')->where($map)->limit($pageLimit)->order('record_id DESC')->select();
      $Page = new Page($totalNum, $pagePer);
      // 设置特殊的ajax方式的分页链接...
      $Page->setAjaxPage("javascript:getPage($theSubject, '$theType', %PAGE%)");
      $my_page = $Page->show();
    }
    $dbSys = D('system')->field('tracker_addr')->find();
    $this->assign('my_tracker', sprintf("http://%s/", $dbSys['tracker_addr']);
    $this->assign('my_rec', $arrRec);
  }*/
  //
  // 获取摄像头在线状态标志 => 这种访问方式效率太低，容易发生堵塞，已经不用了...
  // 2017.06.14 - by jackey => 通道状态直接从数据库获取，避免从采集端获取状态造成的堵塞情况...
  /*private function getCameraStatusFromTransmit($arrGather, &$arrSchool)
  {
    // 尝试链接中转服务器...
    $dbSys = D('system')->field('transmit_addr,transmit_port')->find();*/

    /*// 按照采集端进行查询...
    foreach($arrGather as &$dbGather) {
      // 这里需要先将摄像头集合编号清空...
      unset($arrIDS);
      // 收集采集端下面的摄像头编号集合...
      $map['gather_id'] = $dbGather['gather_id'];
      $arrCamera = D('camera')->where($map)->field('camera_id')->select();
      foreach($arrCamera as &$dbCamera) {
        $arrIDS[] = $dbCamera['camera_id'];
      }
      // 将数据转换成字符串...
      $arrIDS["mac_addr"] = $dbGather['mac_addr'];
      $saveJson = json_encode($arrIDS);
      // 查询指定摄像头列表的在线状态...
      $json_data = php_transmit_command($dbSys['transmit_addr'], $dbSys['transmit_port'], kClientPHP, kCmd_PHP_Get_Camera_Status, $saveJson);
      if( $json_data ) {
        // 获取的JSON数据有效，转成数组，并判断有没有错误码...
        $arrData = json_decode($json_data, true);
        if( is_array($arrData) && !isset($arrData['err_code']) ) {
          // 数组是有效的状态列表，将状态设置到对应的摄像头对象下面...
          foreach($arrSchool as &$dbSchool) {
            // 学校编号不一致，直接下一个...
            if( $dbSchool['school_id'] != $dbGather['school_id'] )
              continue;
            // 学校下面的摄像头列表...
            foreach($dbSchool['data'] as &$dbItem) {
              $dbItem['status'] = 0;
              // 摄像头编号与返回值比较...
              foreach($arrData as $key => $value) {
                if( $dbItem['camera_id'] == $key ) {
                  $dbItem['status'] = $value;
                  break;
                }
              }
            }
          }
        }
      }
    }*/
    
    // 通过php扩展插件连接中转服务器 => 性能高...
    /*$transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
    // 链接中转服务器失败，直接返回...
    if( !$transmit ) return;
    // 按照采集端进行查询...
    foreach($arrGather as &$dbGather) {
      // 这里需要先将摄像头集合编号清空...
      unset($arrIDS);
      // 收集采集端下面的摄像头编号集合...
      $map['gather_id'] = $dbGather['gather_id'];
      $arrCamera = D('camera')->where($map)->field('camera_id')->select();
      foreach($arrCamera as &$dbCamera) {
        $arrIDS[] = $dbCamera['camera_id'];
      }
      // 将数据转换成字符串...
      $arrIDS["mac_addr"] = $dbGather['mac_addr'];
      $saveJson = json_encode($arrIDS);
      // 查询指定摄像头列表的在线状态...
      $json_data = transmit_command(kClientPHP, kCmd_PHP_Get_Camera_Status, $transmit, $saveJson);
      if( $json_data ) {
        // 获取的JSON数据有效，转成数组，并判断有没有错误码...
        $arrData = json_decode($json_data, true);
        if( is_array($arrData) && !isset($arrData['err_code']) ) {
          // 数组是有效的状态列表，将状态设置到对应的摄像头对象下面...
          foreach($arrSchool as &$dbSchool) {
            // 学校编号不一致，直接下一个...
            if( $dbSchool['school_id'] != $dbGather['school_id'] )
              continue;
            // 学校下面的摄像头列表...
            foreach($dbSchool['data'] as &$dbItem) {
              $dbItem['status'] = 0;
              // 摄像头编号与返回值比较...
              foreach($arrData as $key => $value) {
                if( $dbItem['camera_id'] == $key ) {
                  $dbItem['status'] = $value;
                  break;
                }
              }
            }
          }
        }
      }
    }
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
  }*/
  /**
  +----------------------------------------------------------
  * 直播页面...
  +----------------------------------------------------------
  */
  public function live()
  {
    // 获取直播导航数据...
    $my_nav = $this->getNavData(NAV_ACTIVE_LIVE, 0);
    // 对模板对象进行赋值...
    $this->assign('my_title', $this->m_webTitle . ' - 直播');
    $this->assign('my_nav', $my_nav);

    // 2017.06.14 - by jackey => 直接从数据库中读取通道状态，避免从采集端读取造成的阻塞...
    $arrSchool = D('school')->order('school_id ASC')->select();
    foreach($arrSchool as &$dbSchool) {
      $map['school_id'] = $dbSchool['school_id'];
      $dbSchool['data'] = D('LiveView')->where($map)->order('camera_id ASC')->select();
    }
    
    /*// 查找以学校为分类的摄像头列表...
    $arrGather = array();
    $arrSchool = D('school')->order('school_id ASC')->select();
    foreach($arrSchool as &$dbSchool) {
      $map['school_id'] = $dbSchool['school_id'];
      $dbSchool['data'] = D('LiveView')->where($map)->order('camera_id ASC')->select();
      $arrTemp = D('gather')->where($map)->field('gather_id,school_id,mac_addr')->select();
      // 只有当数组有效时，才进行合并...
      if( count($arrTemp) > 0 ) {
        $arrGather = array_merge($arrTemp, $arrGather);
      }
    }
    // 得到直播摄像头的状态信息...
    $this->getCameraStatusFromTransmit($arrGather, $arrSchool);*/
    
    // 设置模板参数...
    $this->assign('my_list', $arrSchool);
    $this->display('live');
  }
  /**
  +----------------------------------------------------------
  * 录像播放页面...
  +----------------------------------------------------------
  */
  public function play()
  {
    // 设定能够显示的播放记录条数...
    $nPageSize = 10;
    // 判断获取传递过来的参数信息...
    if( isset($_GET['record_id']) && isset($_GET['subject_id']) ) {
      // 点播模式 => 播放录像文件...
      $record_id = $_GET['record_id'];
      $subject_id = $_GET['subject_id'];
      // 获取播放页导航数据...
      $my_nav = $this->getNavData(NAV_ACTIVE_SUBJECT, $subject_id);
      // 以当前记录向后查找，升序排列，当前记录为第一个...
      $map['Record.subject_id'] = $subject_id;
      $map['Record.record_id'] = array('egt', $record_id);
      $arrVod = D('RecordView')->where($map)->limit($nPageSize)->order('record_id ASC')->select();
      $curPlay = $arrVod[0];
      // 记录数不够，继续向前查询，降序排列...
      if( count($arrVod) < $nPageSize ) {
        $map['Record.record_id'] = array('lt', $record_id);
        $arrPrev = D('RecordView')->where($map)->limit($nPageSize-count($arrBack))->order('record_id DESC')->select();
        // 有新数据才进行合并，保持当前记录为第一个记录...
        if( count($arrPrev) > 0 ) {
          $arrVod = array_merge($arrVod, $arrPrev);
        }
      }
      // 准备播放标题栏需要的数据...
      $my_base['subject_id'] = $subject_id;
      $my_base['subject_title'] = $my_nav['subject_title'];
      $my_base['play_title'] = sprintf("%s %s %s %s %s %s", $curPlay['grade_type'], $curPlay['grade_name'], $curPlay['camera_name'], $curPlay['teacher_name'], $curPlay['title_name'], $curPlay['created']);
      // 设置点播模板参数...
      $this->assign('my_base', $my_base);
      $this->assign('my_play', $arrVod);
    }
    // 设置其它模板参数...
    $dbSys = D('system')->field('tracker_addr')->find();
    $this->assign('my_tracker', sprintf("http://%s/", $dbSys['tracker_addr']));
    $this->assign('my_title', $this->m_webTitle . ' - 录像播放');
    $this->assign('my_nav', $my_nav);
    $this->display('play');
  }
  /**
  +----------------------------------------------------------
  * 直播播放页面...
  +----------------------------------------------------------
  */
  public function camera()
  {
    // 获取播放页导航数据...
    $my_nav = $this->getNavData(NAV_ACTIVE_LIVE, 0);
    // 直播模式 => 播放直播列表...
    $camera_id = $_GET['camera_id'];
    $status_id = $_GET['status_id'];
    $map['camera_id'] = $camera_id;
    $arrLive = D('LiveView')->where($map)->select();
    foreach($arrLive as &$dbItem) {
      $dbItem['image'] = ($status_id > 0) ? "live-on.png" : "live-off.png";
    }
    // 准备播放标题栏需要的数据...
    $curPlay = $arrLive[0];
    $my_play_title = sprintf("%s %s %s %s", $curPlay['school_name'], $curPlay['grade_type'], $curPlay['grade_name'], $curPlay['camera_name']);
    $this->assign('my_play_title', $my_play_title);
    $this->assign('my_play', $arrLive);
    // 设置其它模板参数...
    $this->assign('my_title', $this->m_webTitle . ' - 直播播放');
    $this->assign('my_nav', $my_nav);
    $this->display('camera');
  }
  /**
  +----------------------------------------------------------
  * 显示页面 => vod | live | width | height
  +----------------------------------------------------------
  */
  public function show()
  {
    // 根据type类型获取url地址...
    if( strcasecmp($_GET['type'], "vod") == 0 ) {
      // 获取点播记录信息...
      $dbSys = D('system')->field('tracker_addr')->find();
      $dbVod = D('record')->where('record_id='.$_GET['id'])->field('file_fdfs,clicks')->find();
      $dbShow['url'] = sprintf("http://%s/%s", $dbSys['tracker_addr'], $dbVod['file_fdfs']);
      $dbShow['type'] = "video/mp4";
      // 累加点播计数器，写入数据库...
      $dbSave['clicks'] = intval($dbVod['clicks']) + 1;
      $dbSave['record_id'] = $_GET['id'];
      D('record')->save($dbSave);
    } else if( strcasecmp($_GET['type'], "live") == 0 ) {
      // 中转服务器需要的参数...
      $dbParam['mac_addr'] = $_GET['mac_addr'];
      $dbParam['rtmp_live'] = $_GET['camera_id'];
      // 首先，判断通道是否处于直播状态...
      $map['camera_id'] = $_GET['camera_id'];
      $dbCamera = D('camera')->where($map)->field('camera_id,clicks,status')->find();
      if( $dbCamera['status'] <= 0 ) {
        $this->dispError('当前通道处于离线状态，无法播放！', '请联系管理员，开启通道。');
        return;
      }
      // 获取直播链接地址...
      $dbResult = $this->getRtmpUrlFromTransmit($dbParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $this->dispError($dbResult['err_msg'], '请联系管理员，汇报错误信息。');
        return;
      }
      // 连接中转服务器成功 => 设置rtmp地址...
      $dbShow['url'] = $dbResult['rtmp_url'];
      $dbShow['type'] = "rtmp/flv";
      // 累加点播计数器，写入数据库...
      $dbCamera['clicks'] = intval($dbCamera['clicks']) + 1;
      D('camera')->save($dbCamera);
    }
    // 获取传递过来的视频窗口大小...
    $dbShow['width'] = isset($_GET['width']) ? ($_GET['width'] - 2) : 840;
    $dbShow['height'] = isset($_GET['height']) ? ($_GET['height'] - 3) : 545;
    //print_r($dbShow); exit;
    // 赋值给播放器模板页面...
    $this->assign('my_show', $dbShow);
    $this->display('show');
  }
  //
  // 显示超时模板信息...
  private function dispError($inMsgTitle, $inMsgDesc)
  {
    $this->assign('my_title', '糟糕，出错了');
    $this->assign('my_msg_title', $inMsgTitle);
    $this->assign('my_msg_desc', $inMsgDesc);
    $this->display('Common:error_page');
  }
  //
  // 2017.06.15 - by jackey => 修改了中转服务器代码，转发命令给采集端之后，立即返回rtmp地址，无需等待采集端上传结果，避免阻塞，让播放器自己去等待...
  // 从中转服务器获取直播地址...
  // 成功 => array()
  // 失败 => false
  private function getRtmpUrlFromTransmit(&$dbParam)
  {
    // 尝试链接中转服务器...
    $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
    
    /*// 获取直播频道所在的URL地址...
    $saveJson = json_encode($dbParam);
    $json_data = php_transmit_command($dbSys['transmit_addr'], $dbSys['transmit_port'], kClientPlay, kCmd_Play_Login, $saveJson);
    // 获取的JSON数据有效，转成数组，并判断有没有错误码...
    $arrData = json_decode($json_data, true);
    return $arrData;
    if( !is_array($arrData) || ($arrrData['err_code'] > 0) )
      return;
    // 判断获取到的URL是否有效...
    if( !isset($arrData['rtmp_url']) )
      return;
    // 返回最终获取到的URL地址...
    return $arrData['rtmp_url'];*/
    
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