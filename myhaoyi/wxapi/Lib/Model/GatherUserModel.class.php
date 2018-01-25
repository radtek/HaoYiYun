<?php

class GatherUserModel extends ViewModel {
    public $viewFields = array(
        'Gather'=>array('gather_id','user_id','_type'=>'LEFT'),
        'User'=>array('user_id','wx_unionid','wx_nickname','wx_headurl','_on'=>'User.user_id=Gather.user_id'),
    );
}
?>