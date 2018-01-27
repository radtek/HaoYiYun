<?php
/*************************************************************
    HaoYi (C)2017 - 2018 myhaoyi.com
    备注：专门处理微信小程序请求页面...
*************************************************************/
/**
 * error code 说明.
 * <ul>
 *    <li>-41001: encodingAesKey 非法</li>
 *    <li>-41003: aes 解密失败</li>
 *    <li>-41004: 解密后得到的buffer非法</li>
 *    <li>-41005: base64加密失败</li>
 *    <li>-41016: base64解密失败</li>
 * </ul>
 */
class ErrorCode
{
	public static $OK = 0;
	public static $IllegalAesKey = -41001;
	public static $IllegalIv = -41002;
	public static $IllegalBuffer = -41003;
	public static $DecodeBase64Error = -41004;
}
/**
 * 对微信小程序用户加密数据的解密示例代码.
 *
 * @copyright Copyright (c) 1998-2014 Tencent Inc.
 */
class WXBizDataCrypt
{
  private $appid;
  private $sessionKey;

	/**
	 * 构造函数
	 * @param $sessionKey string 用户在小程序登录后获取的会话密钥
	 * @param $appid string 小程序的appid
	 */
	public function WXBizDataCrypt( $appid, $sessionKey)
	{
		$this->sessionKey = $sessionKey;
		$this->appid = $appid;
	}
	/**
	 * 检验数据的真实性，并且获取解密后的明文.
	 * @param $encryptedData string 加密的用户数据
	 * @param $iv string 与用户数据一同返回的初始向量
	 * @param $data string 解密后的原文
     *
	 * @return int 成功0，失败返回对应的错误码
	 */
	public function decryptData( $encryptedData, $iv, &$data )
	{
		if (strlen($this->sessionKey) != 24) {
			return ErrorCode::$IllegalAesKey;
		}
		$aesKey=base64_decode($this->sessionKey);
		if (strlen($iv) != 24) {
			return ErrorCode::$IllegalIv;
		}
		$aesIV=base64_decode($iv);

		$aesCipher=base64_decode($encryptedData);

		$result=openssl_decrypt( $aesCipher, "AES-128-CBC", $aesKey, 1, $aesIV);

		$dataObj=json_decode( $result );
		if( $dataObj  == NULL )
		{
			return ErrorCode::$IllegalBuffer;
		}
		if( $dataObj->watermark->appid != $this->appid )
		{
			return ErrorCode::$IllegalBuffer;
		}
		$data = $result;
		return ErrorCode::$OK;
	}
}
///////////////////////////////////////////////
// 微信小程序需要用到的数据接口类...
///////////////////////////////////////////////
class MiniAction extends Action
{
  public function _initialize() {
    $this->m_weMini = C('WECHAT_MINI');
  }
  //
  // 获取小程序的access_token的值 => 附带返回绑定的微信用户信息...
  public function getToken()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性...
      if( !isset($_POST['gather_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 获取绑定的微信用户信息...
      $dbUser = D('GatherUser')->where($_POST)->find();
      if( !isset($dbUser['gather_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '没有找到指定的采集端';
        break;
      }
      // 准备请求需要的url地址...
      $strTokenUrl = sprintf("https://api.weixin.qq.com/cgi-bin/token?grant_type=client_credential&appid=%s&secret=%s",
                           $this->m_weMini['appid'], $this->m_weMini['appsecret']);
      // 直接通过标准API获取access_token...
      $result = http_get($strTokenUrl);
      // 获取access_token失败的情况...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取access_token失败';
        break;
      }
      // 将获取的数据转换成数组...
      $json = json_decode($result,true);
      if( !$json || isset($json['errcode']) ) {
        $arrErr['err_code'] = $json['errcode'];
        $arrErr['err_msg'] = $json['errmsg'];
        break;
      }
      // 获取access_token成功...
      $arrErr['access_token'] = $json['access_token'];
      $arrErr['expires_in'] = $json['expires_in'];
      // 保存用户相关信息 => 将头像替换成132*132尺寸...
      $arrErr['user_id'] = $dbUser['user_id'];
      $arrErr['gather_id'] = $dbUser['gather_id'];
      $arrErr['user_name'] = $dbUser['wx_nickname'];
      $arrErr['user_head'] = str_replace('/0', '/132', $dbUser['wx_headurl']);
      $arrErr['mini_path'] = 'pages/bind/bind';
    } while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序登录事件...
  public function login()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => 没有设置或数据为空，返回错误...
      if( !isset($_POST['code']) || !isset($_POST['encrypt']) || !isset($_POST['iv']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 准备请求需要的url地址...
      $strUrl = sprintf("https://api.weixin.qq.com/sns/jscode2session?appid=%s&secret=%s&js_code=%s&grant_type=authorization_code",
                         $this->m_weMini['appid'], $this->m_weMini['appsecret'], $_POST['code']);
      // code 换取 session_key，判断返回结果...
      $result = http_get($strUrl);
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取openid失败';
        break;
      }
      // 解析微信API返回数据，发生错误...
      $json = json_decode($result,true);
      if( !$json || isset($json['errcode']) ) {
        $arrErr['err_code'] = $json['errcode'];
        $arrErr['err_msg'] = $json['errmsg'];
        break;
      }
      // 获取到了正确的 openid | session_key | expires_in，构造解密对象...
      $wxCrypt = new WXBizDataCrypt($this->m_weMini['appid'], $json['session_key']);
      $theErr = $wxCrypt->decryptData($_POST['encrypt'], $_POST['iv'], $outData);
      // 解码失败，返回错误...
      if( $theErr != 0 ) {
        $arrErr['err_code'] = $theErr;
        $arrErr['err_msg'] = '数据解密失败';
        break;
      }
      // 将获取的数据转换成数组 => 有些字段包含大写字母...
      $arrUser = json_decode($outData, true);
      if( !isset($arrUser['unionId']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '没有获取到unionid';
        break;
      }
      // 微信昵称中去除emoji表情符号的操作...
      $arrUser['nickName'] = trimEmo($arrUser['nickName']);
      // 将获取到的用户关键值查找数据库内容...
      $where['wx_unionid'] = $arrUser['unionId'];
      $dbUser = D('user')->where($where)->find();
      // 从微信获取的信息更新到数据库当中...
      // 这里是小程序，注意有些字段有大写字母，字段标识也有区别...
      $dbUser['wx_unionid'] = $arrUser['unionId'];    // 全局唯一ID
      $dbUser['wx_openid_mini'] = $arrUser['openId']; // 本应用的openid
      $dbUser['wx_nickname'] = $arrUser['nickName'];  // 微信昵称
      $dbUser['wx_language'] = $arrUser['language'];  // 语言
      $dbUser['wx_headurl'] = $arrUser['avatarUrl'];  // 0,46,64,96,132
      $dbUser['wx_country'] = $arrUser['country'];    // 国家
      $dbUser['wx_province'] = $arrUser['province'];  // 省份
      $dbUser['wx_city'] = $arrUser['city'];          // 城市
      $dbUser['wx_sex'] = $arrUser['gender'];         // 性别
      // 更新 $_POST 传递过来的其它数据到数据库对象当中 => 设置了字段才更新保存...
      if( isset($_POST['wx_brand']) ) { $dbUser['wx_brand'] = $_POST['wx_brand']; }
      if( isset($_POST['wx_model']) ) { $dbUser['wx_model'] = $_POST['wx_model']; }
      if( isset($_POST['wx_version']) ) { $dbUser['wx_version'] = $_POST['wx_version']; }
      if( isset($_POST['wx_system']) ) { $dbUser['wx_system'] = $_POST['wx_system']; }
      if( isset($_POST['wx_platform']) ) { $dbUser['wx_platform'] = $_POST['wx_platform']; }
      if( isset($_POST['wx_SDKVersion']) ) { $dbUser['wx_SDKVersion'] = $_POST['wx_SDKVersion']; }
      if( isset($_POST['wx_pixelRatio']) ) { $dbUser['wx_pixelRatio'] = $_POST['wx_pixelRatio']; }
      if( isset($_POST['wx_screenWidth']) ) { $dbUser['wx_screenWidth'] = $_POST['wx_screenWidth']; }
      if( isset($_POST['wx_screenHeight']) ) { $dbUser['wx_screenHeight'] = $_POST['wx_screenHeight']; }
      if( isset($_POST['wx_fontSizeSetting']) ) { $dbUser['wx_fontSizeSetting'] = $_POST['wx_fontSizeSetting']; }
      // 根据id字段判断是否有记录...
      if( isset($dbUser['user_id']) ) {
        // 更新已有的用户记录...
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        $condition['user_id'] = $dbUser['user_id'];
        D('user')->where($condition)->save($dbUser);
      } else {
        // 新建一条用户记录...
        $dbUser['create_time'] = date('Y-m-d H:i:s');
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        $insertid = D('user')->add($dbUser);
        $dbUser['user_id'] = $insertid;
      }
      // 返回得到的用户编号...
      $arrErr['user_id'] = $dbUser['user_id'];
    }while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序请求共享通道接口...
  public function getShare()
  {
    // 得到每页条数...
    $pagePer = C('PAGE_PER');
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取共享通道列表数据 => 视图数据 => 只选择WAN节点
    $condition['node_wan'] = array('gt', 0);
    // 获取记录总数和总页数 => 移除的记录并不影响查询，只是显示时会少一些记录...
    $totalNum = D('TrackView')->where($condition)->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 填充需要返回的信息...
    $arrShare['total_num'] = $totalNum;
    $arrShare['max_page'] = $max_page;
    $arrShare['cur_page'] = $pageCur;
    // 按照分享时间倒叙 => 最后分享的最先显示
    $arrTrack = D('TrackView')->where($condition)->limit($pageLimit)->order("Track.created DESC")->select();
    // 遍历通道，从对应节点获取通道详细实时信息...
    foreach($arrTrack as $key => &$dbItem) {
      // 准备访问节点接口地址 => 获取需要的通道数据记录...
      $strUrl = sprintf("%s://%s/wxapi.php/Mini/getShare/camera_id/%d", $dbItem['node_proto'], $dbItem['node_addr'], $dbItem['camera_id']);
      // 调用接口，返回查询结果...
      $result = http_get($strUrl);
      // 调用失败，在记录中移除...
      if( !$result ) {
        unset($arrTrack[$key]);
        continue;
      }
      // 解析返回的数据记录...
      $arrJson = json_decode($result, true);
      // 返回错误，在记录中移除...
      if( $arrJson['err_code'] > 0 ) {
        unset($arrTrack[$key]);
        continue;
      }
      // 如果在节点中没有找到对应的通道，则在记录中移除 => 后期可以考虑，直接在数据库中删除这个共享通道，避免数据冗余...
      if( !is_array($arrJson['track']) ) {
        unset($arrTrack[$key]);
        continue;
      }
      // 将获取到的通道信息合并到当前通道当中...
      $dbItem = array_merge($dbItem, $arrJson['track']);
      // 对通道拥有者的头像进行缩小处理 => 缩小成 96*96
      $dbItem['wx_headurl'] = str_replace('/0', '/96', $dbItem['wx_headurl']);
    }
    // 将数组的序号重排，否则，会造成小程序的js出错...
    if( is_array($arrTrack) ) {
      $arrTrack = array_merge($arrTrack);
    }
    // 填充返回的通道数据 => 移除的记录并不影响查询，只是显示时会少一些记录...
    $arrShare['track'] = $arrTrack;
    // 返回最终的json数据包...
    echo json_encode($arrShare);
  }
  //
  // 处理小程序验证共享通道接口...
  public function checkShare()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_GET 数据...
    do {
      // 判断输入参数的有效性 => track_id
      if( !isset($_GET['track_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 在数据库中查找指定的共享通道是否存在...
      $condition['track_id'] = $_GET['track_id'];
      $dbTrack = D('track')->where($condition)->find();
      // 如果没有查找到记录，返回错误标志...
      $arrErr['err_code'] = (isset($dbTrack['track_id']) ? false : true);
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序请求的通道直播地址...
  public function getLiveAddr()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => camera_id | node_proto | node_addr...
      if( !isset($_POST['camera_id']) || !isset($_POST['node_proto']) || !isset($_POST['node_addr']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 准备访问节点接口地址 => 获取通道直播地址...
      $strUrl = sprintf("%s://%s/wxapi.php/Mini/getLiveAddr/camera_id/%d", 
                         $_POST['node_proto'], $_POST['node_addr'], $_POST['camera_id']);
      // 调用接口，返回查询结果...
      $result = http_get($strUrl);
      // 调用失败，返回错误信息...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取直播地址失败';
        break;
      }
      // 解析返回的数据记录...
      $arrJson = json_decode($result, true);
      // 返回错误，通知小程序...
      if( $arrJson['err_code'] > 0 ) {
        $arrErr['err_code'] = $arrJson['err_code'];
        $arrErr['err_msg'] = (isset($arrJson['err_msg']) ? $arrJson['err_msg'] : '获取直播地址失败');
        break;
      }
      // 将结果直接赋值返回...
      $arrErr = $arrJson;
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序请求通道下的录像接口...
  public function getRecord()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => cur_page | camera_id | node_proto | node_addr ...
      if( !isset($_POST['cur_page']) || !isset($_POST['camera_id']) || 
          !isset($_POST['node_proto']) || !isset($_POST['node_addr']) )
      {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 准备访问节点接口地址 => 获取通道下的相关录像...
      $strUrl = sprintf("%s://%s/wxapi.php/Mini/getRecord/camera_id/%d/p/%d", 
                        $_POST['node_proto'], $_POST['node_addr'], 
                        $_POST['camera_id'], $_POST['cur_page']);
      // 调用接口，返回查询结果...
      $result = http_get($strUrl);
      // 调用失败，返回错误信息...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取录像记录失败';
        break;
      }
      // 解析返回的数据记录...
      $arrJson = json_decode($result, true);
      // 发生错误，通知小程序...
      if( $arrJson['err_code'] > 0 ) {
        $arrErr['err_code'] = $arrJson['err_code'];
        $arrErr['err_msg'] = (isset($arrJson['err_msg']) ? $arrJson['err_msg'] : '获取录像记录失败');
        break;
      }
      // 将结果直接赋值返回...
      $arrErr = $arrJson;
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序通道在线汇报接口...
  public function liveVerify()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => node_proto | node_addr | rtmp_live | player_id | player_type | player_active
      if( !isset($_POST['node_proto']) || !isset($_POST['node_addr']) || !isset($_POST['rtmp_live']) ||
          !isset($_POST['player_id']) || !isset($_POST['player_type']) || !isset($_POST['player_active']) )
      {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 准备访问节点接口地址 => 汇报通道在线情况...
      $strUrl = sprintf("%s://%s/wxapi.php/RTMP/verify", $_POST['node_proto'], $_POST['node_addr']);
      // 移除不需要的汇报参数...
      unset($_POST['node_addr']);
      unset($_POST['node_proto']);
      // 调用post接口，返回汇报结果...
      $result = http_post($strUrl, $_POST);
      // 调用失败，返回错误信息...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '通道汇报状态失败';
        break;
      }
      // 解析反馈数据，直接赋值...
      $arrErr = json_decode($result, true);
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 处理小程序点击次数累加保存...
  public function saveClick()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => node_proto | node_addr | record_id
      if( !isset($_POST['node_proto']) || !isset($_POST['node_addr']) || !isset($_POST['record_id']) )
      {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 准备访问节点接口地址 => 汇报通道在线情况...
      $strUrl = sprintf("%s://%s/wxapi.php/Mini/saveClick/record_id/%d", $_POST['node_proto'], $_POST['node_addr'], $_POST['record_id']);
      // 调用接口，返回查询结果...
      $result = http_get($strUrl);
      // 调用失败，返回错误信息...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '保存点击次数失败';
        break;
      }
      // 解析返回的数据记录...
      $arrErr = json_decode($result, true);
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 绑定采集端接口命令 => 有三个子命令...
  public function bindGather()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => node_proto | node_addr | bind_cmd | mac_addr
      if( !isset($_POST['node_proto']) || !isset($_POST['node_addr']) || 
          !isset($_POST['bind_cmd']) || !isset($_POST['mac_addr']) ||
          !isset($_POST['gather_id']) || !isset($_POST['user_id']) )
      {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 如果是保存命令，将信息存放到数据库当中...
      // bind_cmd == 1 => Scan
      // bind_cmd == 2 => Save
      // bind_cmd == 3 => Cancel
      if( $_POST['bind_cmd'] == 2 ) {
        $dbSave['gather_id'] = $_POST['gather_id'];
        $dbSave['user_id'] = $_POST['user_id'];
        $dbSave['updated'] = date('Y-m-d H:i:s');
        D('gather')->save($dbSave);
        // 获取绑定用户，转发给采集端...
        $condition['user_id'] = $_POST['user_id'];
        $dbUser = D('user')->where($condition)->field('user_id,wx_nickname,wx_headurl')->find();
        // 保存用户相关信息 => 将头像替换成132*132尺寸...
        $_POST['user_name'] = $dbUser['wx_nickname'];
        $_POST['user_head'] = str_replace('/0', '/132', $dbUser['wx_headurl']);
      }
      // 准备访问节点接口地址 => 通知采集端绑定的状态...
      $strUrl = sprintf("%s://%s/wxapi.php/Mini/bindGather", $_POST['node_proto'], $_POST['node_addr']);
      // 移除不需要的汇报参数...
      unset($_POST['gather_id']);
      unset($_POST['node_addr']);
      unset($_POST['node_proto']);
      // 调用post接口，返回汇报结果...
      $result = http_post($strUrl, $_POST);
      // 调用失败，返回错误信息...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '汇报绑定状态失败';
        break;
      }
      // 解析返回的数据记录...
      $arrErr = json_decode($result, true);
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 解除绑定采集端...
  public function unbindGather()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性...
      if( !isset($_POST['user_id']) || !isset($_POST['gather_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 找到对应的采集端，然后将user_id设置为0...
      $dbSave['user_id'] = 0;
      $dbSave['updated'] = date('Y-m-d H:i:s');
      D('gather')->where($_POST)->save($dbSave);
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 获取指定采集端编号的内容...
  public function findGather()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_GET 数据...
    do {
      // 判断输入参数的有效性...
      if( !isset($_GET['gather_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 查询指定的采集端详细信息...
      $dbGather = D('GatherView')->where($_GET)->find();
      // 如果没有找到，直接返回错误...
      if( !isset($dbGather['gather_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '无法找到指定采集端';
        break;
      }
      // 保存并返回采集端数据...
      $arrErr['dbGather'] = $dbGather;
    } while( false );    
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 获取指定用户拥有的采集端列表...
  public function getGather()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_GET 数据...
    do {
      // 判断输入参数的有效性...
      if( !isset($_GET['user_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 读取采集端列表 => 视图数据 => 只选择指定用户的WAN节点的采集端列表...
      if( !isset($_GET['full']) || $_GET['full'] <= 0 ) {
        $condition['node_wan'] = array('gt', 0);
      }
      // 当设置了 full 参数，并且 full > 0，查询全部包含内网的采集端...
      $condition['user_id'] = $_GET['user_id'];
      $arrGather = D('GatherView')->where($condition)->order('Gather.created DESC')->select();
      // 保存并返回采集端列表...
      $arrErr['list'] = $arrGather;
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 获取指定采集端下面的通道列表 => 分页显示...
  public function getCamera()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => cur_page | mac_addr | node_proto | node_addr ...
      if( !isset($_POST['cur_page']) || !isset($_POST['mac_addr']) || 
          !isset($_POST['node_proto']) || !isset($_POST['node_addr']) )
      {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 准备访问节点接口地址 => 获取通道下的相关录像...
      $strUrl = sprintf("%s://%s/wxapi.php/Mini/getCamera/mac_addr/%s/p/%d", 
                        $_POST['node_proto'], $_POST['node_addr'], 
                        $_POST['mac_addr'], $_POST['cur_page']);
      // 调用接口，返回查询结果...
      $result = http_get($strUrl);
      // 调用失败，返回错误信息...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '获取通道记录失败';
        break;
      }
      // 解析返回的数据记录...
      $arrJson = json_decode($result, true);
      // 发生错误，通知小程序...
      if( $arrJson['err_code'] > 0 ) {
        $arrErr['err_code'] = $arrJson['err_code'];
        $arrErr['err_msg'] = (isset($arrJson['err_msg']) ? $arrJson['err_msg'] : '获取通道记录失败');
        break;
      }
      // 将结果直接赋值返回...
      $arrErr = $arrJson;
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
  //
  // 保存通道的共享状态...
  public function saveShare()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里使用的是 $_POST 数据...
    do {
      // 判断输入参数的有效性 => user_id | node_id | node_proto | node_addr | camera_id | shared...
      if( !isset($_POST['node_proto']) || !isset($_POST['node_addr']) ||
          !isset($_POST['node_id']) || !isset($_POST['user_id']) || 
          !isset($_POST['camera_id']) || !isset($_POST['shared']) )
      {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 先查找是否已经有共享记录...
      $condition['user_id'] = $_POST['user_id'];
      $condition['node_id'] = $_POST['node_id'];
      $condition['camera_id'] = $_POST['camera_id'];
      $dbTrack = D('track')->where($condition)->find();
      // 根据id字段判断是否有记录...
      if( isset($dbTrack['track_id']) ) {
        // 找到了共享记录...
        if( $_POST['shared'] <= 0 ) {
          // 不共享了，直接删除找到的记录...
          D('track')->delete($dbTrack['track_id']);
        } else {
          // 还要继续共享，更新找到的记录...
          $dbTrack['updated'] = date('Y-m-d H:i:s');
          D('track')->save($dbTrack);
        }
      } else {
        // 没有找到共享记录 => 只处理继续共享的状态...
        if( $_POST['shared'] > 0 ) {
          $dbTrack['user_id'] = $_POST['user_id'];
          $dbTrack['node_id'] = $_POST['node_id'];
          $dbTrack['camera_id'] = $_POST['camera_id'];
          $dbTrack['created'] = date('Y-m-d H:i:s');
          $dbTrack['updated'] = date('Y-m-d H:i:s');
          $dbTrack['track_id'] = D('track')->add($dbTrack);
        }
      }
      // 向节点服务器存放通道的共享状态...
      $strUrl = sprintf("%s://%s/wxapi.php/Mini/saveShare/camera_id/%d/shared/%d", 
                        $_POST['node_proto'], $_POST['node_addr'],
                        $_POST['camera_id'], $_POST['shared']);
      // 调用接口，返回查询结果...
      $result = http_get($strUrl);
      // 调用失败，返回错误信息...
      if( !$result ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '保存共享状态失败';
        break;
      }
      // 解析返回的数据记录...
      $arrJson = json_decode($result, true);
      // 发生错误，通知小程序...
      if( $arrJson['err_code'] > 0 ) {
        $arrErr['err_code'] = $arrJson['err_code'];
        $arrErr['err_msg'] = (isset($arrJson['err_msg']) ? $arrJson['err_msg'] : '保存共享状态失败');
        break;
      }
      // 将结果直接赋值返回...
      $arrErr = $arrJson;
    } while( false );
    // 返回最终的json数据包...
    echo json_encode($arrErr);
  }
}
?>