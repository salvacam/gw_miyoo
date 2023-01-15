local unit1 = system.loadunit 'unit1'
local forms = system.loadunit 'forms'
local controls = system.loadunit 'controls'

unit1.form1.oncreate()

unit1.pfs_chance.data = system.loadbin( 'Chance.pcm' )
unit1.pfs_got.data = system.loadbin( 'Got.pcm' )
unit1.pfs_miss1.data = system.loadbin( 'Miss1.pcm' )
unit1.pfs_miss2.data = system.loadbin( 'Miss2.pcm' )
unit1.pfs_miss3.data = system.loadbin( 'Miss3.pcm' )
unit1.pfs_tick.data = system.loadbin( 'Tick.pcm' )

unit1.bsound = true
unit1.imode = 1
unit1.form1.gam_set_mode()

local zoom = { left = 175, top = 103, width = 309, height = 193 }
local left, right = system.splith( zoom )

return system.init{
  background = unit1.form1.im_background,
  zoom = zoom,

  controls = {
    {
      button = unit1.form1.btn_game_a_top,
      label  = 'Game A'
    },
    {
      button = unit1.form1.btn_game_b_top,
      label  = 'Game B'
    },
    {
      button = unit1.form1.btn_time_top,
      label  = 'Time'
    },
    {
      button = unit1.form1.btn_acl_top,
      label  = 'ACL'
    },
    {
      button = unit1.form1.btn_left_down,
      zone   = left,
      label  = 'Left',
      keys   = { left = true },
      xkeys  = { forms.vk_left }
    },
    {
      button = unit1.form1.btn_right_down,
      zone   = right,
      label  = 'Right',
      keys   = { right = true, a = true },
      xkeys  = { forms.vk_right }
    }
  },

  timers = {
    unit1.form1.timer_chance_time,
    unit1.form1.timer_clock,
    unit1.form1.timer_game,
    unit1.form1.timer_game_over,
    unit1.form1.timer_miss
  },

  onkey = function( key, pressed )
    local handler = pressed and unit1.form1.onkeydown or unit1.form1.onkeyup
    handler( nil, key, 0 )
  end,

  onbutton = function( button, pressed )
    local handler = pressed and button.onmousedown or button.onmouseup
    handler( nil, controls.mbleft, false, 0, 0 )
  end
}
