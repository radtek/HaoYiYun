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
  // 处理小程序登录事件...
  public function login()
  {
    // 准备返回信息...
    $arrErr['err_code'] = 0;
    $arrErr['err_msg'] = 'ok';
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
}
?>