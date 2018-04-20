<?php

class TeacherViewModel extends ViewModel {
    public $viewFields = array(
        'Teacher'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Title'=>array('title_id','title_name','_on'=>'Teacher.title_id=Title.title_id'),
    );
}
?>