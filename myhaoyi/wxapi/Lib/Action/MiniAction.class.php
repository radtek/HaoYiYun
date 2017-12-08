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
  // 处理小程序登录事件...
  public function login()
  {
    // 准备返回信息...
    $arrErr['errcode'] = 0;
    $arrErr['errmsg'] = 'ok';
    do {
      // 判断输入参数的有效性 => 没有设置或数据为空，返回错误...
      if( !isset($_POST['code']) || !isset($_POST['encrypt']) || !isset($_POST['iv']) ) {
        $arrErr['errcode'] = true;
        $arrErr['errmsg'] = '输入的参数无效';
        break;
      }
      // 准备请求需要的url地址...
      $strUrl = sprintf("https://api.weixin.qq.com/sns/jscode2session?appid=%s&secret=%s&js_code=%s&grant_type=authorization_code",
                         $this->m_weMini['appid'], $this->m_weMini['appsecret'], $_POST['code']);
      // code 换取 session_key，判断返回结果...
      $result = http_get($strUrl);
      if( !$result ) {
        $arrErr['errcode'] = true;
        $arrErr['errmsg'] = '获取openid失败';
        break;
      }
      // 解析返回数据，发生错误，直接返回...
			$json = json_decode($result,true);
			if( !$json || isset($json['errcode']) ) {
				$arrErr['errcode'] = $json['errcode'];
				$arrErr['errmsg'] = $json['errmsg'];
        break;
      }
      // 获取到了正确的 openid | session_key | expires_in，构造解密对象...
      $wxCrypt = new WXBizDataCrypt($this->m_weMini['appid'], $json['session_key']);
      $errCode = $wxCrypt->decryptData($_POST['encrypt'], $_POST['iv'], $outData);
      // 解码失败，返回错误...
      if( $errCode != 0 ) {
        $arrErr['errcode'] = $errCode;
        $arrErr['errmsg'] = '数据解密失败';
        break;
      }
      // 将获取的数据转换成数组 => 有些字段包含大写字母...
      $arrUser = json_decode($outData, true);
      if( !isset($arrUser['unionId']) ) {
        $arrErr['errcode'] = true;
        $arrErr['errmsg'] = '没有获取到unionid';
        break;
      }
      // 微信昵称中去除emoji表情符号的操作...
      $arrUser['nickName'] = trimEmo($arrUser['nickName']);
      // 将获取到的用户关键帧查找数据库内容...
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
  // 处理小程序请求共享通道接口...
  public function getShare()
  {
    // 得到每页条数...
    $pagePer = C('PAGE_PER');
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取共享通道列表数据 => 视图数据 => 只选择WAN节点...
    $condition['node_wan'] = array('gt', 0);
    $arrTrack = D('TrackView')->where($condition)->limit($pageLimit)->select();
    $arrList = array_unique(array_column($arrTrack, 'node_id'));
    $arrRemote = array();
    for($i = 0; $i < count($arrList); ++$i) {
      $theCameraList = ''; $theNodeAddr = '';
      foreach($arrTrack as &$dbItem) {
        // 找到相同节点的通道...
        if( $dbItem['node_id'] == $arrList[$i] ) {
          $theCameraList .= $dbItem['camera_id'] . ',';
          // 累加通道编号，获取节点完整地址...
          if( strlen($theNodeAddr) <= 0 ) {
            $theNodeAddr = sprintf("%s://%s/wxapi.php/Mini", $dbItem['node_proto'], $dbItem['node_addr']);
          }
        }
      }
      // 去掉通道编号的最后一个逗号，组合查询节点通道列表地址...
      $theCameraList = rtrim($theCameraList, ",");
      $strUrl = sprintf("%s/getShare/list/%s", $theNodeAddr, $theCameraList);
      // 调用接口，返回查询结果...
      $result = http_get($strUrl);
      if( !$result )
        continue;
      // 解析返回数据记录...
      $arrJson = json_decode($result, true);
      if( $arrJson['errcode'] > 0 )
        continue;
      // 将记录重组成通道记录...
      foreach($arrJson['track'] as &$dbJson) {
        $dbJson['node_id'] = $arrList[$i];
        array_push($arrRemote, $dbJson);
      }
    }
    print_r($arrRemote);
    exit;
    // 需要处理筛选后的记录为空的情况...
    // 获取有效节点列表，然后在准备接口数据...
    // 遍历数组，通过节点地址接口获取通道实际数据...
    /*foreach($arrTrack as &$dbItem) {
      // 准备访问节点接口地址 => 获取需要的通道数据记录...
      $strUrl = sprintf("%s/wxapi.php/Mini/getCamera/camera_id/%d", $dbItem['node_addr'], $dbItem['camera_id']);
      $result = http_get($strUrl);
      // 获取通道失败，设置为空，成功，转成数组...
      $dbItem['camera'] = ($result ? json_decode($result, true) : null);
      // 对通道拥有者的头像进行缩小处理 => 缩小成 96*96
      $dbItem['wx_headurl'] = str_replace('/0', '/96', $dbItem['wx_headurl']);
    }*/
  }
}
?>