import Action
import Node
from Params import error

genpy_str = '${PYTHON} ${SRC} -> ${TGT}'

def genpy_file(self, node):
    basefile = node.m_name[:-6]

    gen_script = node.m_parent.get_file("generate-%s.py" % basefile)
    static_src = node.m_parent.get_file("%s.c" % basefile)

    gen_src = node.m_parent.get_file("%s-gen.c" % basefile)
    if not gen_src:
        gen_src = Node.Node("%s-gen.c" % basefile, node.m_parent)
        node.m_parent.append_build(gen_src)

    gentask = self.create_task('genpy')
    gentask.set_inputs([gen_script, static_src])
    gentask.set_outputs(gen_src)

    cctask = self.create_task('cc')
    cctask.set_inputs(gen_src)
    cctask.set_outputs(gen_src.change_ext('.o'))

def setup(env):
    Action.simple_action('genpy', genpy_str, color='BLUE')

    env.hook('cc', 'GENPY_EXT', genpy_file)

def detect(conf):
        python = conf.find_program('python', var='PYTHON')
        if not python:
            error("Could not find python. What the fuck?")
            return 0
        conf.env['GENPY_EXT'] = ['.genpy']
        return 1
