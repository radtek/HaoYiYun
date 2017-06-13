<?php

class GatherViewModel extends ViewModel {
    public $viewFields = array(
        'Gather'=>array('gather_id','school_id','max_camera','mac_addr','ip_addr','name_pc','name_set','created','updated','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'School'=>array('school_id','name'=>'school_name','addr'=>'school_addr','_on'=>'Gather.school_id=School.school_id'),
    );
}
?>