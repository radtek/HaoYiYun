<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class GatherAction extends Action
{
  // 初始化页面的默认操作...
  public function _initialize() {
  }
  /**
  +--------------------------------------------------------------------
  * 处理默认index事件 => 添加或修改gather记录 => 返回系统配置信息...
  +--------------------------------------------------------------------
  */
  public function index()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理 => 测试 => $_GET;
    $arrData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($arrData['mac_addr']) || !isset($arrData['ip_addr']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "MAC地址或IP地址为空！";
        break;
      }
      // 根据MAC地址获取Gather记录信息...
      $dbGather = D('gather')->where('mac_addr="'.$arrData['mac_addr'].'"')->find();
      if( count($dbGather) <= 0 ) {
        // 没有找到记录，直接创建一个新记录...
        $dbGather = $arrData;
        $dbGather['created'] = date('Y-m-d H:i:s');
        $dbGather['updated'] = date('Y-m-d H:i:s');
        $arrErr['gather_id'] = D('gather')->add($dbGather);
      } else {
        // 找到了记录，直接更新记录...
        $arrErr['gather_id'] = $dbGather['gather_id'];
        $arrData['gather_id'] = $dbGather['gather_id'];
        $arrData['updated'] = date('Y-m-d H:i:s');
        D('gather')->save($arrData);
      }
      // 读取系统配置表，返回给采集端...
      $dbSys = D('system')->field('tracker_addr,tracker_port,transmit_addr,transmit_port')->find();
      $arrErr['tracker_addr'] = $dbSys['tracker_addr'];
      $arrErr['tracker_port'] = strval($dbSys['tracker_port']);
      $arrErr['transmit_addr'] = $dbSys['transmit_addr'];
      $arrErr['transmit_port'] = strval($dbSys['transmit_port']);
      $arrErr['local_time'] = date('Y-m-d H:i:s');
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  /**
  +----------------------------------------------------------
  * 处理采集端退出 => 将所有采集端下面的通知状态设置为0...
  +----------------------------------------------------------
  */
  public function logout()
  {
    // 判断输入的采集端编号是否有效...
    if( !isset($_POST['gather_id']) )
      return;
    // 将所有该采集端下面的通道状态设置为0...
    $map['gather_id'] = $_POST['gather_id'];
    D('camera')->where($map)->setField('status', 0);
  }
  /**
  +----------------------------------------------------------
  * 处理摄像头注册事件 => 添加或修改camera记录...
  +----------------------------------------------------------
  */
  public function camera()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理...
    $arrData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($arrData['gather_id']) || !isset($arrData['device_sn']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "采集端编号或设备序列号为空！";
        break;
      }
      // 根据设备序列号获取Camera记录信息...
      $condition['device_sn'] = $arrData['device_sn'];
      $dbCamera = D('camera')->where($condition)->find();
      if( count($dbCamera) <= 0 ) {
        // 没有找到记录，直接创建一个新记录 => 返回camera_id和camera_name...
        $dbCamera = $arrData;
        $dbCamera['created'] = date('Y-m-d H:i:s');
        $dbCamera['updated'] = date('Y-m-d H:i:s');
        $arrErr['camera_name'] = $arrData['camera_name'];
        $arrErr['camera_id'] = strval(D('camera')->add($dbCamera));
      } else {
        // 如果grade_id有效，则需要组合年级名称...
        if( $dbCamera['grade_id'] > 0 ) {
          $map['grade_id'] = $dbCamera['grade_id'];
          $dbGrade = D('grade')->where($map)->field('grade_type,grade_name')->find();
          $arrErr['camera_name'] = sprintf("%s %s %s", $dbGrade['grade_type'], $dbGrade['grade_name'], $dbCamera['camera_name']);
        } else {
          $arrErr['camera_name'] = $dbCamera['camera_name'];
        }
        // 找到了记录，直接更新记录 => 返回camera_id和camera_name...
        $arrErr['camera_id'] = strval($dbCamera['camera_id']);
        $arrData['camera_name'] = $dbCamera['camera_name'];
        $arrData['camera_id'] = $dbCamera['camera_id'];
        $arrData['updated'] = date('Y-m-d H:i:s');
        D('camera')->save($arrData);
      }
      // 读取该通道下的所有录像课程表，反馈给采集端...
      $newMap['camera_id'] = $arrErr['camera_id'];
      $arrCourse = D('course')->where($newMap)->field('course_id,camera_id,subject_id,teacher_id,repeat_id,elapse_sec,start_time,end_time')->select();
      // 将字符串时间转换成整数时间戳...
      foreach($arrCourse as &$dbItem) {
        $dbItem['start_time'] = strval(strtotime($dbItem['start_time']));
        $dbItem['end_time'] = strval(strtotime($dbItem['end_time']));
      }
      // 将课程记录保存到返回队列当中...
      $arrErr['course'] = $arrCourse;
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  /**
  +----------------------------------------------------------
  * 处理保存摄像头删除过程 => 返回 camera_id ...
  +----------------------------------------------------------
  */
  public function delCamera()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    do {
      // 判断输入的 device_sn 是否有效...
      if( !isset($_POST['device_sn']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "设备序列号为空！";
        break;
      }
      // 直接通过条件删除摄像头...
      $map['device_sn'] = $_POST['device_sn'];
      $dbCamera = D('camera')->where($map)->field('camera_id')->find();
      D('camera')->where($map)->delete();
      // 这里还需要删除对应的录像课程表记录...
      D('course')->where($dbCamera)->delete();
      // 返回摄像头在数据库中的编号...
      $arrErr['camera_id'] = $dbCamera['camera_id'];
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  /**
  +----------------------------------------------------------
  * 处理保存摄像头信息 => 参数 camera_id | status ...
  +----------------------------------------------------------
  */
  public function saveCamera()
  {
    /*// 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理...
    $dbData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($dbData['camera_id']) || !isset($dbData['status']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "摄像头编号不能为空！";
        break;
      }
      // 直接保存摄像头数据...
      $arrErr['camera_id'] = $dbData['camera_id'];
      $dbData['updated'] = date('Y-m-d H:i:s');
      D('camera')->save($dbData);
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);*/

    // 直接保存，不返回结果...
    $_POST['updated'] = date('Y-m-d H:i:s');
    D('camera')->save($_POST);
  }
  /**
  +----------------------------------------------------------
  * 处理保存录像记录过程...
  +----------------------------------------------------------
  */
  public function saveFDFS()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理...$_GET;//
    $arrData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($arrData['ext']) || !isset($arrData['file_src']) || !isset($arrData['file_fdfs']) || !isset($arrData['file_size']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "文件名称或文件长度不能为空！";
        break;
      }
      // 将file_src进行切分...
      $arrSrc = explode('_', $arrData['file_src']);
      // 组合通用数据项...
      $arrData['file_src'] = (is_null($arrSrc[0]) ? $arrData['file_src'] : $arrSrc[0]);
      $arrData['camera_id'] = (is_null($arrSrc[1]) ? 0 : $arrSrc[1]);
      $arrData['created'] = date('Y-m-d H:i:s');
      $arrData['updated'] = date('Y-m-d H:i:s');
      // 根据文件扩展名进行数据表分发...
      // jpg => uniqid_DBCameraID
      // mp4 => uniqid_DBCameraID_CourseID_Duration
      if( (strcasecmp($arrData['ext'], ".jpg") == 0) || (strcasecmp($arrData['ext'], ".jpeg") == 0) ) {
        // 查找截图记录...
        $dbImage = D('image')->where('file_src="'.$arrData['file_src'].'"')->find();
        if( is_array($dbImage) ) {
          // 更新截图记录...
          $dbImage['camera_id'] = $arrData['camera_id'];
          $dbImage['file_fdfs'] = $arrData['file_fdfs'];
          $dbImage['file_size'] = $arrData['file_size'];
          $dbImage['updated'] = $arrData['updated'];
          D('image')->save($dbImage);
          $arrErr['image_id'] = $dbImage['image_id'];
        } else {
          // 新增截图记录...
          $arrErr['image_id'] = D('image')->add($arrData);
        }
        // 在录像表中查找file_src，找到了，则更新image_id，截图匹配...
        $dbRec = D('record')->where('file_src="'.$arrData['file_src'].'"')->field('record_id,image_id')->find();
        if( is_array($dbRec) ) {
          $dbRec['image_id'] = $arrErr['image_id'];
          $dbRec['updated'] = $arrData['updated'];
          D('record')->save($dbRec);
        }
      } else if( strcasecmp($arrData['ext'], ".mp4") == 0 ) {
        // 保存录像时长(秒)，初始化image_id...
        $arrData['image_id'] = 0;
        $arrData['course_id'] = (is_null($arrSrc[2]) ? 0 : $arrSrc[2]);
        $arrData['duration'] = (is_null($arrSrc[3]) ? 0 : $arrSrc[3]);
        $nTotalSec = intval($arrData['duration']);
        $theSecond = intval($nTotalSec % 60);
        $theMinute = intval($nTotalSec / 60) % 60;
        $theHour = intval($nTotalSec / 3600);
        if( $theHour > 0 ) {
          $arrData['disptime'] = sprintf("%02d:%02d:%02d", $theHour, $theMinute, $theSecond);
        } else {
          $arrData['disptime'] = sprintf("%02d:%02d", $theMinute, $theSecond);
        }
        // 如果course_id有效，则需要查找到subject_id和teacher_id...
        if( $arrData['course_id'] > 0 ) {
          $dbCourse = D('course')->where('course_id='.$arrData['course_id'])->field('subject_id,teacher_id')->find();
          $arrData['subject_id'] = (isset($dbCourse['subject_id']) ? $dbCourse['subject_id'] : 0);
          $arrData['teacher_id'] = (isset($dbCourse['teacher_id']) ? $dbCourse['teacher_id'] : 0);
        }
        // 在图片表中查找file_src，找到了，则新增image_id，截图匹配...
        $dbImage = D('image')->where('file_src="'.$arrData['file_src'].'"')->field('image_id,camera_id')->find();
        if( is_array($dbImage) ) {
          $arrData['image_id'] = $dbImage['image_id'];
        }
        // 查找录像记录...
        $dbRec = D('record')->where('file_src="'.$arrData['file_src'].'"')->find();
        if( is_array($dbRec) ) {
          // 更新录像记录...
          $dbRec['subject_id'] = $arrData['subject_id'];
          $dbRec['teacher_id'] = $arrData['teacher_id'];
          $dbRec['image_id'] = $arrData['image_id'];
          $dbRec['camera_id'] = $arrData['camera_id'];
          $dbRec['file_fdfs'] = $arrData['file_fdfs'];
          $dbRec['file_size'] = $arrData['file_size'];
          $dbRec['duration'] = $arrData['duration'];
          $dbRec['disptime'] = $arrData['disptime'];
          $dbRec['updated'] = $arrData['updated'];
          D('record')->save($dbRec);
          $arrErr['record_id'] = $dbRec['record_id'];
        } else {
          // 新增录像记录...
          $arrErr['record_id'] = D('record')->add($arrData);
        }
      }
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
}
?>